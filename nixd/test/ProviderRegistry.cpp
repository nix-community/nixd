#include "nixd/Controller/ProviderRegistry.h"
#include "nixd/Controller/Configuration.h"
#include "nixd/Controller/ProviderQuery.h"
#include "nixd/Eval/AttrSetClient.h"
#include "nixd/Support/ProcessTree.h"

#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>

#include <llvm/Support/Error.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <set>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace nixd {
namespace {

class ManualExecutor {
  boost::asio::io_context Context;

public:
  ProviderRegistry::Executor executor() {
    return ProviderRegistry::Executor(Context.get_executor());
  }

  bool runOne() {
    Context.restart();
    return Context.poll_one() != 0;
  }

  void runAll() {
    Context.restart();
    Context.poll();
  }
};

struct WorkerTrace {
  std::mutex Mutex;
  std::vector<std::thread::id> DestructionThreads;
};

class FakeWorker final : public ProviderWorker {
public:
  struct Evaluation {
    std::string Expression;
    EvaluationCallback Reply;
  };

private:
  DeathCallback OnDeath;
  std::shared_ptr<WorkerTrace> Trace;
  std::atomic<bool> Alive{true};

public:
  std::vector<Evaluation> Evaluations;
  unsigned CancelCount = 0;
  std::function<void()> CancelAction;
  bool ThrowOnEvaluate = false;
  bool ThrowOnCancel = false;

  FakeWorker(DeathCallback OnDeath, std::shared_ptr<WorkerTrace> Trace)
      : OnDeath(std::move(OnDeath)), Trace(std::move(Trace)) {}

  ~FakeWorker() override {
    std::lock_guard Guard(Trace->Mutex);
    Trace->DestructionThreads.push_back(std::this_thread::get_id());
  }

  void evaluate(std::string Expression, EvaluationCallback Reply) override {
    if (ThrowOnEvaluate)
      throw std::runtime_error("evaluate failed synchronously");
    Evaluations.push_back({std::move(Expression), std::move(Reply)});
  }

  void cancel() override {
    ++CancelCount;
    Alive = false;
    if (CancelAction)
      CancelAction();
    if (ThrowOnCancel)
      throw std::runtime_error("cancel failed synchronously");
  }

  [[nodiscard]] bool alive() const override { return Alive; }

  EvaluationCallback takeReply(size_t Index) {
    return std::move(Evaluations.at(Index).Reply);
  }

  void finish(size_t Index, bool Success) { takeReply(Index)(Success); }

  void die() {
    Alive = false;
    OnDeath();
  }
};

class FakeFactory {
  std::shared_ptr<WorkerTrace> Trace = std::make_shared<WorkerTrace>();

public:
  struct CreatedWorker {
    ProviderKey Key;
    std::filesystem::path CWD;
    std::weak_ptr<FakeWorker> Worker;
  };

  std::vector<CreatedWorker> Created;
  std::optional<ProviderKey> ThrowOnCreate;
  std::optional<ProviderKey> ThrowOnEvaluate;
  std::optional<ProviderKey> ThrowOnCancel;

  ProviderRegistry::WorkerFactory factory() {
    return [this](const ProviderKey &Key, const std::filesystem::path &CWD,
                  ProviderWorker::DeathCallback OnDeath) {
      if (ThrowOnCreate == Key)
        throw std::runtime_error("factory failed synchronously");
      auto Worker = std::make_shared<FakeWorker>(std::move(OnDeath), Trace);
      Worker->ThrowOnEvaluate = ThrowOnEvaluate == Key;
      Worker->ThrowOnCancel = ThrowOnCancel == Key;
      Created.push_back({Key, CWD, Worker});
      return Worker;
    };
  }

  std::shared_ptr<FakeWorker> worker(const ProviderKey &Key,
                                     size_t Generation = 0) {
    for (const auto &Entry : Created) {
      if (Entry.Key != Key)
        continue;
      if (Generation == 0)
        return Entry.Worker.lock();
      --Generation;
    }
    return {};
  }

  std::shared_ptr<WorkerTrace> trace() { return Trace; }
};

class CoordinatedFakeWorker final : public ProviderWorker {
  std::shared_ptr<ProcessTreeIdentity> Identity;
  const ProcessTreeBackend &Backend;

public:
  unsigned CancelCount = 0;
  unsigned PrepareCount = 0;
  unsigned FinishCount = 0;

  CoordinatedFakeWorker(pid_t PID, const ProcessTreeBackend &Backend)
      : Identity(std::make_shared<ProcessTreeIdentity>(PID, PID)),
        Backend(Backend) {}

  void evaluate(std::string, EvaluationCallback Reply) override { Reply(true); }

  void cancel() override {
    ++CancelCount;
    const std::array Trees{Identity};
    cancelProcessTrees(Trees, Backend);
    finishCancellation();
  }

  std::shared_ptr<ProcessTreeIdentity> prepareCancellation() override {
    ++PrepareCount;
    return Identity;
  }

  void finishCancellation() noexcept override {
    ++FinishCount;
    (void)Identity->markReaped();
  }

  [[nodiscard]] bool alive() const override { return Identity->ownsIdentity(); }
};

class TermIgnoringEvaluatorWorker final : public ProviderWorker {
  std::unique_ptr<AttrSetClientProc> Process;
  bool ChildReady = false;

public:
  explicit TermIgnoringEvaluatorWorker(DeathCallback OnDeath) {
    int Ready[2];
    if (::pipe(Ready) != 0)
      return;
    const std::array ChildFDs{Ready[1]};
    try {
      Process = std::make_unique<AttrSetClientProc>(
          [ReadFD = Ready[0], WriteFD = Ready[1]] {
            (void)::close(ReadFD);
            (void)::signal(SIGTERM, SIG_IGN);
            const char Byte = 'R';
            ssize_t Written;
            do {
              Written = ::write(WriteFD, &Byte, 1);
            } while (Written < 0 && errno == EINTR);
            (void)::close(WriteFD);
            for (;;)
              ::pause();
            return 0;
          },
          std::move(OnDeath), ChildFDs);
    } catch (...) {
      (void)::close(Ready[0]);
      (void)::close(Ready[1]);
      throw;
    }
    (void)::close(Ready[1]);
    char Byte = 0;
    ssize_t Read;
    do {
      Read = ::read(Ready[0], &Byte, 1);
    } while (Read < 0 && errno == EINTR);
    (void)::close(Ready[0]);
    ChildReady = Read == 1 && Byte == 'R';
  }

  void evaluate(std::string, EvaluationCallback Reply) override { Reply(true); }

  void cancel() override {
    if (Process)
      (void)Process->stop();
  }

  std::shared_ptr<ProcessTreeIdentity> prepareCancellation() override {
    return Process ? Process->prepareStop() : nullptr;
  }

  void finishCancellation() noexcept override {
    if (Process)
      Process->finishStop();
  }

  [[nodiscard]] bool alive() const override {
    return Process && Process->alive();
  }

  [[nodiscard]] pid_t pid() const { return Process ? Process->pid() : -1; }
  [[nodiscard]] bool ready() const { return ChildReady; }
};

class ExactEvaluatorCleanup {
  pid_t PID;

public:
  explicit ExactEvaluatorCleanup(pid_t PID) : PID(PID) {}

  ~ExactEvaluatorCleanup() {
    if (PID <= 0)
      return;
    siginfo_t Info{};
    errno = 0;
    if (::waitid(P_PID, PID, &Info, WEXITED | WNOHANG | WNOWAIT) < 0 &&
        errno == ECHILD)
      return;
    (void)::kill(-PID, SIGKILL);
    (void)::kill(PID, SIGKILL);
    int Status = 0;
    while (::waitpid(PID, &Status, 0) < 0 && errno == EINTR) {
    }
  }
};

class ExactEvaluatorWatchdog {
  std::vector<pid_t> PIDs;
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Completed = false;
  std::atomic<bool> Fired = false;
  std::thread Thread;

public:
  explicit ExactEvaluatorWatchdog(std::vector<pid_t> PIDs)
      : PIDs(std::move(PIDs)), Thread([this] {
          std::unique_lock Lock(Mutex);
          if (Changed.wait_for(Lock, std::chrono::seconds(5),
                               [this] { return Completed; }))
            return;
          Fired = true;
          for (pid_t PID : this->PIDs) {
            (void)::kill(-PID, SIGKILL);
            (void)::kill(PID, SIGKILL);
          }
        }) {}

  void complete() {
    {
      std::lock_guard Guard(Mutex);
      Completed = true;
    }
    Changed.notify_all();
  }

  [[nodiscard]] bool fired() const { return Fired; }

  ~ExactEvaluatorWatchdog() {
    complete();
    Thread.join();
  }
};

ProviderSpec nixpkgs(std::string Expression) {
  ProviderSpec Spec;
  Spec.Nixpkgs = std::move(Expression);
  return Spec;
}

ProviderSpec option(std::string Name, std::string Expression) {
  ProviderSpec Spec;
  Spec.Options.emplace(std::move(Name), std::move(Expression));
  return Spec;
}

TEST(ProviderRegistry, ActivatesOnlyTheMatchingPendingRevision) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(nixpkgs("expr-a"));
  Executor.runAll();

  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Pending);
  ASSERT_EQ(Factory.Created.size(), 1);
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  ASSERT_EQ(Worker->Evaluations.size(), 1);
  EXPECT_EQ(Worker->Evaluations[0].Expression, "expr-a");

  Worker->finish(0, true);
  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Pending);
  Executor.runAll();

  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Active);
  EXPECT_EQ(Registry.epochs().Nixpkgs, 2);
}

TEST(ProviderRegistry, IgnoresStaleEvaluationCompletion) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(nixpkgs("expr-a"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  auto StaleReply = Worker->takeReply(0);

  Registry.apply(nixpkgs("expr-b"));
  Executor.runAll();
  ASSERT_EQ(Worker->Evaluations.size(), 2);

  StaleReply(true);
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Pending);

  Worker->finish(1, true);
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Active);
}

TEST(ProviderRegistry, DeduplicatesPendingAndRetriesFailedProviders) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(option("nixos", "expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(Worker);
  const auto PendingEpoch = Registry.epochs().Options;

  Registry.apply(option("nixos", "expr"));
  Executor.runAll();
  EXPECT_EQ(Worker->Evaluations.size(), 1);
  EXPECT_EQ(Registry.epochs().Options, PendingEpoch);

  Worker->finish(0, false);
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Failed);
  EXPECT_EQ(Registry.epochs().Options, PendingEpoch);

  Registry.apply(option("nixos", "expr"));
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Pending);
  ASSERT_EQ(Worker->Evaluations.size(), 2);
  EXPECT_EQ(Registry.epochs().Options, PendingEpoch);

  Worker->finish(1, true);
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Active);
}

TEST(ProviderRegistry, RetiresRemovedRecordAndInvalidatesItsToken) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(option("nixos", "expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();
  auto Token = Registry.acquire(ProviderKey::option("nixos"));
  ASSERT_TRUE(Token);

  Registry.apply({});
  Executor.runAll();

  EXPECT_EQ(Token->observedState(), ProviderState::Retired);
  EXPECT_FALSE(Registry.validate(*Token));
  EXPECT_FALSE(Registry.state(ProviderKey::option("nixos")));
  EXPECT_EQ(Worker->CancelCount, 1);
}

TEST(ProviderRegistry, ReclaimsRemovedWorkerOnTheInjectedExecutor) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(option("nixos", "expr"));
  Executor.runAll();
  ASSERT_TRUE(Factory.worker(ProviderKey::option("nixos")));

  Registry.apply({});
  std::thread::id ExecutorThread;
  std::thread Drain([&] {
    ExecutorThread = std::this_thread::get_id();
    Executor.runAll();
  });
  Drain.join();

  auto Trace = Factory.trace();
  std::lock_guard Guard(Trace->Mutex);
  ASSERT_EQ(Trace->DestructionThreads.size(), 1);
  EXPECT_EQ(Trace->DestructionThreads.front(), ExecutorThread);
}

TEST(ProviderRegistry, RecoversCurrentDeadWorkerUsingStartupCWD) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(option("nixos", "expr"));
  Executor.runAll();
  auto First = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(First);
  First->finish(0, true);
  Executor.runAll();
  const auto ActiveEpoch = Registry.epochs().Options;

  First->die();
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Active);
  Executor.runAll();

  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Pending);
  ASSERT_EQ(Factory.Created.size(), 2);
  EXPECT_EQ(Factory.Created[1].CWD, "/startup/cwd");
  EXPECT_EQ(Registry.epochs().Options, ActiveEpoch + 1);
  auto Second = Factory.worker(ProviderKey::option("nixos"), 1);
  ASSERT_TRUE(Second);
  ASSERT_EQ(Second->Evaluations.size(), 1);
  EXPECT_EQ(Second->Evaluations[0].Expression, "expr");

  Second->finish(0, true);
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Active);
  EXPECT_EQ(Registry.epochs().Options, ActiveEpoch + 2);
}

TEST(ProviderRegistry, MaintainsSeparateEpochsAndIgnoresProviderNoOps) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  ProviderSpec Initial = nixpkgs("nixpkgs-a");
  Initial.Options.emplace("nixos", "options-a");
  Registry.apply(Initial);
  Executor.runAll();
  auto Nixpkgs = Factory.worker(ProviderKey::nixpkgs());
  auto Options = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(Nixpkgs);
  ASSERT_TRUE(Options);
  Nixpkgs->finish(0, true);
  Options->finish(0, true);
  Executor.runAll();
  const auto ActiveEpochs = Registry.epochs();

  Registry.apply(
      Initial); // formatting/diagnostic-only changes reach this no-op.
  Executor.runAll();
  EXPECT_EQ(Registry.epochs(), ActiveEpochs);
  EXPECT_EQ(Nixpkgs->Evaluations.size(), 1);
  EXPECT_EQ(Options->Evaluations.size(), 1);

  Initial.Options["nixos"] = "options-b";
  Registry.apply(Initial);
  Executor.runAll();
  EXPECT_EQ(Registry.epochs().Nixpkgs, ActiveEpochs.Nixpkgs);
  EXPECT_EQ(Registry.epochs().Options, ActiveEpochs.Options + 1);

  Options->finish(1, true);
  Executor.runAll();
  const auto OptionsChanged = Registry.epochs();
  Initial.Nixpkgs = "nixpkgs-b";
  Registry.apply(Initial);
  Executor.runAll();
  EXPECT_EQ(Registry.epochs().Nixpkgs, OptionsChanged.Nixpkgs + 1);
  EXPECT_EQ(Registry.epochs().Options, OptionsChanged.Options);
}

TEST(ProviderRegistry,
     WholeOptionsSnapshotRejectsReconfigureBetweenProviderResults) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  ProviderSpec Initial;
  Initial.Options = {{"a", "a-old"}, {"b", "b-old"}};
  Registry.apply(Initial);
  Executor.runAll();
  Factory.worker(ProviderKey::option("a"))->finish(0, true);
  Factory.worker(ProviderKey::option("b"))->finish(0, true);
  Executor.runAll();

  auto Snapshot = Registry.acquireOptions();
  ASSERT_EQ(Snapshot.size(), 2U);
  std::vector<std::string> Staged{Snapshot[0].key().Name};

  ProviderSpec Replacement;
  Replacement.Options = {{"a", "a-new"}, {"b", "b-old"}};
  Registry.apply(std::move(Replacement));
  Staged.push_back(Snapshot[1].key().Name);

  ASSERT_EQ(Staged, (std::vector<std::string>{"a", "b"}));
  EXPECT_FALSE(Registry.validate(Snapshot));
}

TEST(ProviderRegistry,
     WholeOptionsSnapshotRejectsReconfigureBeforeHoverCommit) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  Registry.apply(option("a", "a-old"));
  Executor.runAll();
  Factory.worker(ProviderKey::option("a"))->finish(0, true);
  Executor.runAll();

  auto Snapshot = Registry.acquireOptions();
  ASSERT_EQ(Snapshot.size(), 1U);
  const std::optional<std::string> StagedHover = "hover-a";
  Registry.apply(option("a", "a-new"));

  ASSERT_TRUE(StagedHover);
  EXPECT_FALSE(Registry.validate(Snapshot));
}

TEST(ProviderRegistry, InvalidatesQueryTokenAfterConfigurationChanges) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(nixpkgs("expr-a"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();
  auto Token = Registry.acquire(ProviderKey::nixpkgs());
  ASSERT_TRUE(Token);
  EXPECT_TRUE(Registry.validate(*Token));

  Registry.apply(nixpkgs("expr-b"));

  EXPECT_FALSE(Registry.validate(*Token));
  EXPECT_FALSE(Registry.acquire(ProviderKey::nixpkgs()));
  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Pending);
  Executor.runAll();
}

TEST(ProviderRegistry, InvalidProviderConfigIsUnavailableBeforeRetirementRuns) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  Registry.apply(nixpkgs("expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();
  auto Token = Registry.acquire(ProviderKey::nixpkgs());
  ASSERT_TRUE(Token);

  Configuration Invalid = defaultConfiguration();
  Invalid.nixpkgs.expr.clear();
  Invalid.options.clear();
  Registry.apply(providerSpec(Invalid));

  EXPECT_FALSE(Registry.acquire(ProviderKey::nixpkgs()));
  EXPECT_FALSE(Registry.validate(*Token));
  EXPECT_FALSE(Registry.state(ProviderKey::nixpkgs()));
  EXPECT_EQ(Worker->CancelCount, 0U);

  Executor.runAll();
  EXPECT_EQ(Worker->CancelCount, 1U);
}

TEST(ProviderRegistry, OrdinaryQueryFailureDoesNotTriggerRecovery) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(nixpkgs("expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();
  auto Token = Registry.acquire(ProviderKey::nixpkgs());
  ASSERT_TRUE(Token);
  const auto ActiveEpoch = Registry.epochs().Nixpkgs;

  Registry.queryFailed(*Token);
  Executor.runAll();

  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Active);
  EXPECT_EQ(Registry.epochs().Nixpkgs, ActiveEpoch);
  EXPECT_EQ(Factory.Created.size(), 1);
}

TEST(ProviderRegistry, ConfirmedQueryTransportDeathTriggersRecovery) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  Registry.apply(nixpkgs("expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();
  auto Token = Registry.acquire(ProviderKey::nixpkgs());
  ASSERT_TRUE(Token);

  Worker->die();
  Registry.queryFailed(*Token);
  Executor.runAll();

  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Pending);
  EXPECT_EQ(Factory.Created.size(), 2);
}

TEST(ProviderRegistry, ShutdownGatesCancelsAndRetiresPendingProviders) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  ProviderSpec Initial = nixpkgs("nixpkgs");
  Initial.Options.emplace("nixos", "options");
  Registry.apply(Initial);
  Executor.runAll();
  auto Nixpkgs = Factory.worker(ProviderKey::nixpkgs());
  auto Options = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(Nixpkgs);
  ASSERT_TRUE(Options);
  auto StaleNixpkgsReply = Nixpkgs->takeReply(0);
  auto StaleOptionsReply = Options->takeReply(0);

  bool Retired = false;
  Registry.shutdown([&] { Retired = true; });
  EXPECT_FALSE(Registry.accepting());
  EXPECT_EQ(Nixpkgs->CancelCount, 1);
  EXPECT_EQ(Options->CancelCount, 1);

  Registry.apply(nixpkgs("ignored"));
  Executor.runAll();
  EXPECT_TRUE(Retired);
  EXPECT_EQ(Registry.size(), 0);
  EXPECT_EQ(Factory.Created.size(), 2);

  StaleNixpkgsReply(true);
  StaleOptionsReply(true);
  Executor.runAll();
  EXPECT_EQ(Registry.size(), 0);
}

TEST(ProviderRegistry, ShutdownUsesOneSharedGraceForCoordinatedWorkers) {
  ManualExecutor Executor;
  std::set<pid_t> Alive{7001, 7002, 7003};
  std::chrono::milliseconds GraceTotal(0);
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t Target, int Signal) {
            const pid_t PID = Target < 0 ? -Target : Target;
            if (Signal == 0) {
              if (Alive.contains(PID))
                return 0;
              errno = ESRCH;
              return -1;
            }
            if (Signal == SIGKILL)
              Alive.erase(PID);
            return 0;
          },
      .WaitForGrace =
          [&](std::chrono::milliseconds Delay) { GraceTotal += Delay; },
  };
  std::vector<std::shared_ptr<CoordinatedFakeWorker>> Workers;
  pid_t NextPID = 7001;
  ProviderRegistry Registry(
      Executor.executor(),
      [&](const ProviderKey &, const std::filesystem::path &,
          ProviderWorker::DeathCallback) {
        auto Worker =
            std::make_shared<CoordinatedFakeWorker>(NextPID++, Backend);
        Workers.push_back(Worker);
        return Worker;
      },
      "/startup/cwd", Backend);
  ProviderSpec Spec = nixpkgs("nixpkgs");
  Spec.Options.emplace("one", "options");
  Spec.Options.emplace("two", "options");
  Registry.apply(std::move(Spec));
  Executor.runAll();

  Registry.shutdown();

  EXPECT_EQ(GraceTotal, std::chrono::milliseconds(500));
  ASSERT_EQ(Workers.size(), 3U);
  for (const auto &Worker : Workers) {
    EXPECT_EQ(Worker->PrepareCount, 1U);
    EXPECT_EQ(Worker->CancelCount, 0U);
    EXPECT_EQ(Worker->FinishCount, 1U);
  }
}

TEST(ProviderRegistry, ShutdownUsesOneGraceForRealTermIgnoringEvaluators) {
  ManualExecutor Executor;
  std::vector<std::shared_ptr<TermIgnoringEvaluatorWorker>> Workers;
  ProviderRegistry Registry(
      Executor.executor(),
      [&](const ProviderKey &, const std::filesystem::path &,
          ProviderWorker::DeathCallback OnDeath) {
        auto Worker =
            std::make_shared<TermIgnoringEvaluatorWorker>(std::move(OnDeath));
        Workers.push_back(Worker);
        return Worker;
      },
      std::filesystem::current_path());
  ProviderSpec Spec = nixpkgs("nixpkgs");
  Spec.Options.emplace("one", "options");
  Spec.Options.emplace("two", "options");
  Registry.apply(std::move(Spec));
  Executor.runAll();

  std::vector<std::unique_ptr<ExactEvaluatorCleanup>> Cleanups;
  std::vector<pid_t> PIDs;
  for (const auto &Worker : Workers) {
    PIDs.push_back(Worker->pid());
    Cleanups.push_back(std::make_unique<ExactEvaluatorCleanup>(Worker->pid()));
  }
  EXPECT_EQ(Workers.size(), 3U);
  if (Workers.size() != 3U)
    return;
  for (const auto &Worker : Workers) {
    ASSERT_TRUE(Worker->ready());
    ASSERT_GT(Worker->pid(), 0);
  }
  ExactEvaluatorWatchdog Watchdog(PIDs);

  const auto Start = std::chrono::steady_clock::now();
  Registry.shutdown();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;
  Watchdog.complete();

  EXPECT_GE(Elapsed, std::chrono::milliseconds(400));
  EXPECT_LT(Elapsed, std::chrono::milliseconds(1200));
  EXPECT_FALSE(Watchdog.fired());
  for (pid_t PID : PIDs) {
    siginfo_t Info{};
    errno = 0;
    EXPECT_EQ(::waitid(P_PID, PID, &Info, WEXITED | WNOHANG | WNOWAIT), -1);
    EXPECT_EQ(errno, ECHILD);
  }
}

TEST(ProviderRegistry, ConcurrentShutdownWaitersRunAfterSingleRetirement) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  Registry.apply(nixpkgs("expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);

  std::mutex Mutex;
  std::condition_variable Changed;
  bool CancelStarted = false;
  bool AllowCancel = false;
  Worker->CancelAction = [&] {
    std::unique_lock Lock(Mutex);
    CancelStarted = true;
    Changed.notify_all();
    Changed.wait(Lock, [&] { return AllowCancel; });
  };
  std::vector<size_t> SizesSeenByWaiters;
  auto Waiter = [&] { SizesSeenByWaiters.push_back(Registry.size()); };

  std::thread First([&] { Registry.shutdown(Waiter); });
  {
    std::unique_lock Lock(Mutex);
    ASSERT_TRUE(Changed.wait_for(Lock, std::chrono::seconds(2),
                                 [&] { return CancelStarted; }));
  }
  Registry.shutdown(Waiter);
  EXPECT_TRUE(SizesSeenByWaiters.empty());
  {
    std::lock_guard Guard(Mutex);
    AllowCancel = true;
  }
  Changed.notify_all();
  First.join();

  Executor.runAll();
  ASSERT_EQ(SizesSeenByWaiters.size(), 2);
  EXPECT_EQ(SizesSeenByWaiters[0], 0);
  EXPECT_EQ(SizesSeenByWaiters[1], 0);
  EXPECT_EQ(Worker->CancelCount, 1);

  Registry.shutdown(Waiter);
  EXPECT_EQ(SizesSeenByWaiters.size(), 2);
  Executor.runAll();
  ASSERT_EQ(SizesSeenByWaiters.size(), 3);
  EXPECT_EQ(SizesSeenByWaiters[2], 0);
}

TEST(ProviderRegistry, SynchronousFactoryExceptionFailsOnlyMatchingRevision) {
  ManualExecutor Executor;
  FakeFactory Factory;
  Factory.ThrowOnCreate = ProviderKey::nixpkgs();
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  ProviderSpec Spec = nixpkgs("nixpkgs");
  Spec.Options.emplace("nixos", "options");

  EXPECT_NO_THROW({
    Registry.apply(std::move(Spec));
    Executor.runAll();
  });

  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Failed);
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Pending);
  ASSERT_EQ(Factory.Created.size(), 1);
  auto Options = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(Options);
  Options->finish(0, true);
  Executor.runAll();
  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Active);
}

TEST(ProviderRegistry, SynchronousEvaluateExceptionFailsMatchingRevision) {
  ManualExecutor Executor;
  FakeFactory Factory;
  Factory.ThrowOnEvaluate = ProviderKey::option("nixos");
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  EXPECT_NO_THROW({
    Registry.apply(option("nixos", "options"));
    Executor.runAll();
  });

  EXPECT_EQ(Registry.state(ProviderKey::option("nixos")),
            ProviderState::Failed);
}

TEST(ProviderRegistry, CancellationExceptionsDoNotEscapeShutdown) {
  ManualExecutor Executor;
  FakeFactory Factory;
  Factory.ThrowOnCancel = ProviderKey::nixpkgs();
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  Registry.apply(nixpkgs("nixpkgs"));
  Executor.runAll();
  bool Retired = false;

  EXPECT_NO_THROW(Registry.shutdown([&] { Retired = true; }));
  Executor.runAll();

  EXPECT_TRUE(Retired);
  EXPECT_FALSE(Registry.accepting());
  EXPECT_EQ(Registry.size(), 0);
}

TEST(ProviderQuery, ReturnsResultOnlyWhileTheSnapshotRemainsCurrent) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  Registry.apply(nixpkgs("expr-a"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();

  EXPECT_EQ(queryProvider<int>(
                Registry, ProviderKey::nixpkgs(), -1,
                [](ProviderWorker &) { return llvm::Expected<int>(42); }),
            42);

  EXPECT_EQ(queryProvider<int>(Registry, ProviderKey::nixpkgs(), -1,
                               [&](ProviderWorker &) {
                                 Registry.apply(nixpkgs("expr-b"));
                                 Executor.runAll();
                                 return llvm::Expected<int>(42);
                               }),
            -1);
}

TEST(ProviderQuery, UsesEachFeatureFallbackWhenProviderIsUnavailableOrStale) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");

  const std::vector<int> LocalCompletions{1, 2};
  EXPECT_EQ(queryProvider<std::vector<int>>(
                Registry, ProviderKey::nixpkgs(), LocalCompletions,
                [](ProviderWorker &) {
                  return llvm::Expected<std::vector<int>>(std::vector<int>{9});
                }),
            LocalCompletions);

  const std::string OriginalResolve = "original-item";
  EXPECT_EQ(queryProvider<std::string>(
                Registry, ProviderKey::nixpkgs(), OriginalResolve,
                [](ProviderWorker &) {
                  return llvm::Expected<std::string>("resolved-item");
                }),
            OriginalResolve);

  EXPECT_FALSE(queryProvider<std::optional<int>>(
      Registry, ProviderKey::nixpkgs(), std::nullopt, [](ProviderWorker &) {
        return llvm::Expected<std::optional<int>>(std::optional<int>(9));
      }));

  const std::vector<int> StaticDefinitions{7};
  EXPECT_EQ(queryProvider<std::vector<int>>(
                Registry, ProviderKey::option("nixos"), StaticDefinitions,
                [](ProviderWorker &) {
                  return llvm::Expected<std::vector<int>>(std::vector<int>{8});
                }),
            StaticDefinitions);

  EXPECT_TRUE(queryProvider<std::vector<int>>(
                  Registry, ProviderKey::option("nixos"), {},
                  [](ProviderWorker &) {
                    return llvm::Expected<std::vector<int>>(
                        std::vector<int>{8});
                  })
                  .empty());
}

TEST(ProviderQuery, OrdinaryRPCErrorDoesNotRecoverButDeathDoes) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  Registry.apply(nixpkgs("expr"));
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);
  Worker->finish(0, true);
  Executor.runAll();

  auto ErrorQuery = [](ProviderWorker &) -> llvm::Expected<int> {
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "ordinary rpc error");
  };
  EXPECT_EQ(
      queryProvider<int>(Registry, ProviderKey::nixpkgs(), -1, ErrorQuery), -1);
  Executor.runAll();
  EXPECT_EQ(Factory.Created.size(), 1U);
  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Active);

  Worker->die();
  EXPECT_EQ(
      queryProvider<int>(Registry, ProviderKey::nixpkgs(), -1, ErrorQuery), -1);
  Executor.runAll();
  EXPECT_EQ(Factory.Created.size(), 2U);
  EXPECT_EQ(Registry.state(ProviderKey::nixpkgs()), ProviderState::Pending);
}

TEST(ProviderQuery, AcquiresOptionTokensAndClientLeasesFromOneSnapshot) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  ProviderSpec Spec;
  Spec.Options = {{"nixos", "nixos-expr"}, {"home", "home-expr"}};
  Registry.apply(std::move(Spec));
  Executor.runAll();
  auto NixOS = Factory.worker(ProviderKey::option("nixos"));
  auto Home = Factory.worker(ProviderKey::option("home"));
  ASSERT_TRUE(NixOS);
  ASSERT_TRUE(Home);
  NixOS->finish(0, true);
  Home->finish(0, true);
  Executor.runAll();

  auto Tokens = Registry.acquireOptions();
  ASSERT_EQ(Tokens.size(), 2U);
  EXPECT_EQ(Tokens[0].key(), ProviderKey::option("home"));
  EXPECT_EQ(Tokens[1].key(), ProviderKey::option("nixos"));
  EXPECT_EQ(Tokens[0].client(), nullptr);
  EXPECT_EQ(Tokens[1].client(), nullptr);

  Registry.apply(option("nixos", "changed"));
  EXPECT_FALSE(Registry.validate(Tokens[0]));
  EXPECT_FALSE(Registry.validate(Tokens[1]));
}

TEST(ProviderRegistry, ApplyTicketWaitsForEveryExactRevisionTerminal) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  ProviderSpec Spec = nixpkgs("nixpkgs");
  Spec.Options.emplace("nixos", "options");
  std::vector<ProviderApplyResult> Results;
  std::thread::id CallbackThread;
  Registry.apply(std::move(Spec), [&](ProviderApplyResult Result) {
    Results.push_back(Result);
    CallbackThread = std::this_thread::get_id();
    EXPECT_TRUE(Registry.state(ProviderKey::nixpkgs()));
  });

  Executor.runAll();
  EXPECT_TRUE(Results.empty());
  auto Nixpkgs = Factory.worker(ProviderKey::nixpkgs());
  auto Options = Factory.worker(ProviderKey::option("nixos"));
  ASSERT_TRUE(Nixpkgs);
  ASSERT_TRUE(Options);
  Nixpkgs->finish(0, true);
  Executor.runAll();
  EXPECT_TRUE(Results.empty());
  Options->finish(0, false);
  std::thread::id ExecutorThread;
  std::thread Drain([&] {
    ExecutorThread = std::this_thread::get_id();
    Executor.runAll();
  });
  Drain.join();

  ASSERT_EQ(Results.size(), 1U);
  EXPECT_EQ(Results.front(), ProviderApplyResult::Failed);
  EXPECT_EQ(CallbackThread, ExecutorThread);
  Executor.runAll();
  EXPECT_EQ(Results.size(), 1U);
}

TEST(ProviderRegistry, ApplyTicketHandlesEmptyAndSynchronousFailures) {
  ManualExecutor EmptyExecutor;
  FakeFactory EmptyFactory;
  ProviderRegistry Empty(EmptyExecutor.executor(), EmptyFactory.factory(),
                         "/startup/cwd");
  std::vector<ProviderApplyResult> EmptyResults;
  Empty.apply(
      {}, [&](ProviderApplyResult Result) { EmptyResults.push_back(Result); });
  EXPECT_TRUE(EmptyResults.empty());
  EmptyExecutor.runAll();
  EXPECT_EQ(EmptyResults, std::vector{ProviderApplyResult::Empty});

  ManualExecutor FailureExecutor;
  FakeFactory FailureFactory;
  FailureFactory.ThrowOnCreate = ProviderKey::nixpkgs();
  FailureFactory.ThrowOnEvaluate = ProviderKey::option("nixos");
  ProviderRegistry Failure(FailureExecutor.executor(), FailureFactory.factory(),
                           "/startup/cwd");
  ProviderSpec Spec = nixpkgs("nixpkgs");
  Spec.Options.emplace("nixos", "options");
  std::vector<ProviderApplyResult> FailureResults;
  Failure.apply(std::move(Spec), [&](ProviderApplyResult Result) {
    FailureResults.push_back(Result);
  });
  FailureExecutor.runAll();

  EXPECT_EQ(FailureResults, std::vector{ProviderApplyResult::Failed});
  EXPECT_EQ(Failure.state(ProviderKey::nixpkgs()), ProviderState::Failed);
  EXPECT_EQ(Failure.state(ProviderKey::option("nixos")), ProviderState::Failed);
}

TEST(ProviderRegistry, ApplyTicketTreatsPreActiveDeathAsFailure) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  std::vector<ProviderApplyResult> Results;
  Registry.apply(nixpkgs("expr"), [&](ProviderApplyResult Result) {
    Results.push_back(Result);
  });
  Executor.runAll();
  auto First = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(First);

  First->die();
  Executor.runAll();
  EXPECT_EQ(Results, std::vector{ProviderApplyResult::Failed});
  auto Second = Factory.worker(ProviderKey::nixpkgs(), 1);
  ASSERT_TRUE(Second);
  Second->finish(0, true);
  Executor.runAll();

  EXPECT_EQ(Results.size(), 1U);
}

TEST(ProviderRegistry, ApplyTicketReportsSupersededAndLateSuccessOnce) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  std::vector<ProviderApplyResult> FirstResults;
  Registry.apply(nixpkgs("first"), [&](ProviderApplyResult Result) {
    FirstResults.push_back(Result);
  });
  Executor.runAll();
  auto Worker = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Worker);

  auto Barrier = std::make_shared<std::binary_semaphore>(0);
  std::vector<ProviderApplyResult> SecondResults;
  Registry.apply(nixpkgs("second"), [&, Barrier](ProviderApplyResult Result) {
    SecondResults.push_back(Result);
    Barrier->release();
  });
  EXPECT_FALSE(Barrier->try_acquire_for(std::chrono::milliseconds(1)));
  Executor.runAll();
  EXPECT_EQ(FirstResults, std::vector{ProviderApplyResult::Superseded});
  EXPECT_TRUE(SecondResults.empty());

  ASSERT_EQ(Worker->Evaluations.size(), 2U);
  Worker->finish(1, true);
  Executor.runAll();

  EXPECT_TRUE(Barrier->try_acquire());
  EXPECT_EQ(SecondResults, std::vector{ProviderApplyResult::Ready});
  Executor.runAll();
  EXPECT_EQ(SecondResults.size(), 1U);
}

TEST(ProviderRegistry, ApplyTicketChecksSupersessionBeforeFailure) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  ProviderSpec First = nixpkgs("nixpkgs");
  First.Options.emplace("nixos", "first");
  std::vector<ProviderApplyResult> Results;
  Registry.apply(std::move(First), [&](ProviderApplyResult Result) {
    Results.push_back(Result);
  });
  Executor.runAll();

  auto Nixpkgs = Factory.worker(ProviderKey::nixpkgs());
  ASSERT_TRUE(Nixpkgs);
  Nixpkgs->finish(0, false);
  ProviderSpec Second = nixpkgs("nixpkgs");
  Second.Options.emplace("nixos", "second");
  Registry.apply(std::move(Second));
  Executor.runAll();

  EXPECT_EQ(Results, std::vector{ProviderApplyResult::Superseded});
}

TEST(ProviderRegistry, ShutdownStopsApplyTicketExactlyOnce) {
  ManualExecutor Executor;
  FakeFactory Factory;
  ProviderRegistry Registry(Executor.executor(), Factory.factory(),
                            "/startup/cwd");
  std::vector<ProviderApplyResult> Results;
  Registry.apply(nixpkgs("expr"), [&](ProviderApplyResult Result) {
    Results.push_back(Result);
  });
  Executor.runAll();
  unsigned Retired = 0;
  Registry.shutdown([&] {
    ++Retired;
    EXPECT_EQ(Results, std::vector{ProviderApplyResult::Stopped});
  });
  Executor.runAll();

  EXPECT_EQ(Results, std::vector{ProviderApplyResult::Stopped});
  EXPECT_EQ(Retired, 1U);
  Executor.runAll();
  EXPECT_EQ(Results.size(), 1U);
}

} // namespace
} // namespace nixd
