#include "nixd/Controller/ProviderRegistry.h"

#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
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
  Executor.runAll();

  EXPECT_FALSE(Registry.validate(*Token));
  EXPECT_FALSE(Registry.acquire(ProviderKey::nixpkgs()));
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

} // namespace
} // namespace nixd
