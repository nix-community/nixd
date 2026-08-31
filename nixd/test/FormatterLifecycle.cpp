#include "nixd/Controller/Formatter.h"
#include "nixd/Eval/AttrSetClient.h"
#include "nixd/Support/ForkPiped.h"
#include "nixd/Support/ProcessTree.h"

#include "../lib/Controller/FormatterInternal.h"
#include "ControllerTestPeer.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace nixd {
namespace {

using namespace std::chrono_literals;

size_t countFormatterDescriptors() {
  size_t Count = 0;
  for (int FD = 0; FD < ::getdtablesize(); ++FD) {
    errno = 0;
    if (::fcntl(FD, F_GETFD) != -1 || errno != EBADF)
      ++Count;
  }
  return Count;
}

pid_t waitForLeader(FormatterProcessRegistry &Registry) {
  const auto Deadline = std::chrono::steady_clock::now() + 2s;
  do {
    if (auto PIDs = Registry.activePIDs(); !PIDs.empty())
      return PIDs.front();
    std::this_thread::sleep_for(1ms);
  } while (std::chrono::steady_clock::now() < Deadline);
  return -1;
}

void expectReaped(pid_t PID) {
  int Status = 0;
  errno = 0;
  EXPECT_EQ(::waitpid(PID, &Status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
}

class ExactProcessCleanup {
  pid_t Leader = -1;

public:
  void setLeader(pid_t PID) { Leader = PID; }

  ~ExactProcessCleanup() {
    if (Leader <= 0)
      return;
    siginfo_t Info{};
    errno = 0;
    if (::waitid(P_PID, Leader, &Info, WEXITED | WNOHANG | WNOWAIT) < 0 &&
        errno == ECHILD)
      return;
    (void)::kill(-this->Leader, SIGKILL);
    (void)::kill(this->Leader, SIGKILL);
    int Status = 0;
    while (::waitpid(Leader, &Status, 0) < 0 && errno == EINTR) {
    }
  }
};

class ExactProcessWatchdog {
  pid_t Leader;
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Completed = false;
  std::atomic<bool> Fired = false;
  std::thread Thread;

public:
  explicit ExactProcessWatchdog(pid_t Leader)
      : Leader(Leader), Thread([this] {
          std::unique_lock Lock(Mutex);
          if (Changed.wait_for(Lock, 2s, [this] { return Completed; }))
            return;
          Fired = true;
          siginfo_t Info{};
          errno = 0;
          if (::waitid(P_PID, this->Leader, &Info,
                       WEXITED | WNOHANG | WNOWAIT) < 0 &&
              errno == ECHILD)
            return;
          (void)::kill(-this->Leader, SIGKILL);
          (void)::kill(this->Leader, SIGKILL);
        }) {}

  void complete() {
    {
      std::lock_guard Guard(Mutex);
      Completed = true;
    }
    Changed.notify_all();
  }

  ~ExactProcessWatchdog() {
    complete();
    Thread.join();
  }

  [[nodiscard]] bool fired() const { return Fired; }
};

class ExactDescendantCleanup {
  pid_t PID = -1;

public:
  void setPID(pid_t NewPID) { PID = NewPID; }
  void release() { PID = -1; }

  ~ExactDescendantCleanup() {
    if (PID <= 0)
      return;
    (void)::kill(PID, SIGKILL);
    const auto Deadline = std::chrono::steady_clock::now() + 2s;
    while (::kill(PID, 0) == 0 && std::chrono::steady_clock::now() < Deadline)
      std::this_thread::sleep_for(10ms);
  }
};

class TemporaryPIDFile {
  std::string Path;

public:
  TemporaryPIDFile()
      : TemporaryPIDFile(std::filesystem::temp_directory_path()) {}

  explicit TemporaryPIDFile(const std::filesystem::path &Directory) {
    std::string Template =
        (Directory / "codex-nixd-formatter-pid.XXXXXX").string();
    const int FD = ::mkstemp(Template.data());
    EXPECT_GE(FD, 0);
    if (FD >= 0)
      (void)::close(FD);
    Path = Template;
  }

  ~TemporaryPIDFile() { (void)::unlink(Path.c_str()); }

  [[nodiscard]] const std::string &path() const { return Path; }
};

class TemporaryDirectory {
  std::filesystem::path Path;

public:
  TemporaryDirectory() {
    std::string Template = (std::filesystem::temp_directory_path() /
                            "codex-nixd-formatter-dir.XXXXXX")
                               .string();
    const char *Created = ::mkdtemp(Template.data());
    EXPECT_NE(Created, nullptr);
    if (Created)
      Path = Created;
  }

  ~TemporaryDirectory() {
    std::error_code Error;
    (void)std::filesystem::remove_all(Path, Error);
  }

  [[nodiscard]] const std::filesystem::path &path() const { return Path; }
};

pid_t waitForPIDFile(const TemporaryPIDFile &File) {
  const auto Deadline = std::chrono::steady_clock::now() + 2s;
  do {
    const int FD = ::open(File.path().c_str(), O_RDONLY);
    if (FD >= 0) {
      char Buffer[32]{};
      const ssize_t Read = ::read(FD, Buffer, sizeof(Buffer) - 1);
      (void)::close(FD);
      if (Read > 0) {
        char *End = nullptr;
        const long Parsed = std::strtol(Buffer, &End, 10);
        if (End != Buffer && Parsed > 0)
          return static_cast<pid_t>(Parsed);
      }
    }
    std::this_thread::sleep_for(1ms);
  } while (std::chrono::steady_clock::now() < Deadline);
  return -1;
}

bool waitForPIDGone(pid_t PID) {
  const auto Deadline = std::chrono::steady_clock::now() + 2s;
  do {
    errno = 0;
    if (::kill(PID, 0) < 0 && errno == ESRCH)
      return true;
    std::this_thread::sleep_for(10ms);
  } while (std::chrono::steady_clock::now() < Deadline);
  return false;
}

class OrderingWorker final : public ProviderWorker {
  std::function<bool()> FormattersCancelled;
  std::atomic<bool> &ObservedCancellation;

public:
  OrderingWorker(std::function<bool()> FormattersCancelled,
                 std::atomic<bool> &ObservedCancellation)
      : FormattersCancelled(std::move(FormattersCancelled)),
        ObservedCancellation(ObservedCancellation) {}

  void evaluate(std::string, EvaluationCallback Reply) override { Reply(true); }

  void cancel() override { ObservedCancellation = FormattersCancelled(); }

  [[nodiscard]] bool alive() const override { return true; }
};

std::vector<pid_t> waitForControllerFormatters(Controller &C, size_t Count) {
  const auto Deadline = std::chrono::steady_clock::now() + 5s;
  std::vector<pid_t> PIDs;
  do {
    PIDs = ControllerTestPeer::formatterPIDs(C);
    if (PIDs.size() == Count)
      break;
    std::this_thread::sleep_for(1ms);
  } while (std::chrono::steady_clock::now() < Deadline);
  return PIDs;
}

volatile sig_atomic_t FormatterSIGPIPECount = 0;

extern "C" void countFormatterSIGPIPE(int) {
  FormatterSIGPIPECount = FormatterSIGPIPECount + 1;
}

class ScopedSIGPIPEProbe {
  struct sigaction Previous{};

public:
  ScopedSIGPIPEProbe() {
    FormatterSIGPIPECount = 0;
    struct sigaction Action{};
    Action.sa_handler = countFormatterSIGPIPE;
    sigemptyset(&Action.sa_mask);
    EXPECT_EQ(::sigaction(SIGPIPE, &Action, &Previous), 0);
  }

  ~ScopedSIGPIPEProbe() { (void)::sigaction(SIGPIPE, &Previous, nullptr); }

  [[nodiscard]] sig_atomic_t count() const { return FormatterSIGPIPECount; }
};

TEST(FormatterLifecycle, QueuedBeforeClosedGateNeverLaunches) {
  std::atomic<unsigned> Launches = 0;
  FormatterProcessRegistry Registry;
  Registry.cancelAll();

  auto Process = Registry.launch([&] {
    ++Launches;
    return util::PipedProc(4242, 4242, -1, -1, -1);
  });

  EXPECT_FALSE(Process);
  EXPECT_EQ(Launches, 0U);
}

TEST(FormatterLifecycle, LaunchAndRegistrationAreAtomicWithCancellation) {
  std::promise<void> LaunchEntered;
  std::promise<void> AllowLaunch;
  auto Allowed = AllowLaunch.get_future().share();
  std::atomic<bool> Alive = true;
  std::vector<int> Signals;
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t Target, int Signal) {
            EXPECT_EQ(Target, -4242);
            if (Signal == 0)
              return Alive ? 0 : -1;
            Signals.push_back(Signal);
            if (Signal == SIGKILL)
              Alive = false;
            return 0;
          },
      .WaitForGrace = [](std::chrono::milliseconds) {},
  };
  FormatterProcessRegistry Registry(Backend);

  std::unique_ptr<FormatterProcess> Process;
  std::thread Launcher([&] {
    auto Launched = Registry.launch([&] {
      LaunchEntered.set_value();
      Allowed.wait();
      return util::PipedProc(4242, 4242, -1, -1, -1);
    });
    if (Launched)
      Process = std::make_unique<FormatterProcess>(std::move(*Launched));
  });
  LaunchEntered.get_future().wait();
  std::thread Canceller([&] { Registry.cancelAll(); });
  AllowLaunch.set_value();
  Launcher.join();
  Canceller.join();

  ASSERT_TRUE(Process);
  EXPECT_TRUE(Process->Identity->cancellationRequested());
  EXPECT_EQ(Signals, (std::vector<int>{SIGTERM, SIGKILL}));
  EXPECT_TRUE(Process->Identity->markReaped());
  Registry.deregister(Process->Identity);
}

TEST(FormatterLifecycle, BlockedInputIsCancelledWithoutSIGPIPE) {
  ScopedSIGPIPEProbe SIGPIPEProbe;
  FormatterProcessRegistry Registry;
  std::string Input(2 * 1024 * 1024, 'x');
  auto Run = std::async(std::launch::async, [&] {
    return runFormatter(Registry,
                        {"sh", "-c", "trap '' TERM; exec tail -f /dev/null"},
                        std::filesystem::current_path(), Input);
  });
  ExactProcessCleanup Cleanup;
  const pid_t Leader = waitForLeader(Registry);
  Cleanup.setLeader(Leader);
  ASSERT_GT(Leader, 0);

  Registry.cancelAll();
  ASSERT_EQ(Run.wait_for(2s), std::future_status::ready);
  std::optional<FormatterRunResult> Result;
  try {
    Result = Run.get();
  } catch (const std::exception &Err) {
    ADD_FAILURE() << Err.what();
  }
  ASSERT_TRUE(Result);
  EXPECT_TRUE(Result->Cancelled);
  EXPECT_EQ(SIGPIPEProbe.count(), 0);
  expectReaped(Leader);
}

TEST(FormatterLifecycle, DrainsLargeStdoutAndStderrBeforeWaiting) {
  ScopedSIGPIPEProbe SIGPIPEProbe;
  FormatterProcessRegistry Registry;
  const std::string Chunk(128, 'o');
  const std::string Script = "i=0; while [ $i -lt 1024 ]; do printf '%s' '" +
                             Chunk + "'; printf '%s' '" + Chunk +
                             "' >&2; i=$((i+1)); done; cat";

  auto Run = std::async(std::launch::async, [&] {
    return runFormatter(Registry, {"sh", "-c", Script},
                        std::filesystem::current_path(), "input");
  });
  ExactProcessCleanup Cleanup;
  const pid_t Leader = waitForLeader(Registry);
  Cleanup.setLeader(Leader);
  ASSERT_GT(Leader, 0);
  ExactProcessWatchdog Watchdog(Leader);
  std::optional<FormatterRunResult> Result;
  try {
    Result = Run.get();
  } catch (const std::exception &Err) {
    ADD_FAILURE() << Err.what();
  }
  Watchdog.complete();
  EXPECT_FALSE(Watchdog.fired());
  EXPECT_EQ(SIGPIPEProbe.count(), 0);

  ASSERT_TRUE(Result);
  EXPECT_FALSE(Result->Cancelled);
  EXPECT_EQ(Result->ExitStatus, 0);
  EXPECT_EQ(Result->Stdout.size(), Chunk.size() * 1024 + 5);
  EXPECT_EQ(Result->Stderr.size(), Chunk.size() * 1024);
  expectReaped(Leader);
}

TEST(FormatterLifecycle, ShellHelperTreatsTemporaryPathAsOpaqueArgument) {
  TemporaryDirectory Directory;
  ASSERT_FALSE(Directory.path().empty());
  const std::filesystem::path HostileDirectory =
      Directory.path() / "space ; shell [meta]";
  ASSERT_TRUE(std::filesystem::create_directory(HostileDirectory));
  TemporaryPIDFile PIDFile(HostileDirectory);
  FormatterProcessRegistry Registry;
  auto Run = std::async(std::launch::async, [&] {
    return runFormatter(Registry,
                        {"sh", "-c",
                         "printf '%s' \"$$\" > \"$1\"; sleep 0.2; exit 0",
                         "nixd-test", PIDFile.path()},
                        std::filesystem::current_path(), "input");
  });
  ExactProcessCleanup Cleanup;
  const pid_t Leader = waitForLeader(Registry);
  Cleanup.setLeader(Leader);
  if (Leader <= 0) {
    Registry.cancelAll();
    (void)Run.get();
    FAIL() << "formatter leader was never registered";
  }
  const FormatterRunResult Result = Run.get();
  const pid_t Published = waitForPIDFile(PIDFile);

  EXPECT_EQ(Result.ExitStatus, 0);
  EXPECT_EQ(Published, Leader);
  expectReaped(Leader);
}

TEST(FormatterLifecycle, HostileTreeWithInheritedWritersIsBoundedAndReaped) {
  TemporaryPIDFile DescendantFile;
  ExactDescendantCleanup DescendantCleanup;
  FormatterProcessRegistry Registry;
  auto Run = std::async(std::launch::async, [&] {
    return runFormatter(Registry,
                        {"sh", "-c",
                         "(trap '' TERM; exec tail -f /dev/null) & child=$!; "
                         "printf '%s' \"$child\" > \"$1\"; "
                         "trap '' TERM; exec tail -f /dev/null",
                         "nixd-test", DescendantFile.path()},
                        std::filesystem::current_path(), "input");
  });
  ExactProcessCleanup Cleanup;
  const pid_t Leader = waitForLeader(Registry);
  Cleanup.setLeader(Leader);
  ASSERT_GT(Leader, 0);
  const pid_t Descendant = waitForPIDFile(DescendantFile);
  DescendantCleanup.setPID(Descendant);
  ASSERT_GT(Descendant, 0);

  const auto Start = std::chrono::steady_clock::now();
  Registry.cancelAll();
  ASSERT_EQ(Run.wait_for(2s), std::future_status::ready);
  EXPECT_LT(std::chrono::steady_clock::now() - Start, 2s);
  EXPECT_TRUE(Run.get().Cancelled);
  expectReaped(Leader);
  ASSERT_TRUE(waitForPIDGone(Descendant));
  DescendantCleanup.release();
}

TEST(FormatterLifecycle,
     SuccessfulFormatterCleansBackgroundOwnedGroupBeforeLeaderReap) {
  TemporaryPIDFile DescendantFile;
  ExactDescendantCleanup DescendantCleanup;
  FormatterProcessRegistry Registry;
  const auto Start = std::chrono::steady_clock::now();
  auto Run = std::async(std::launch::async, [&] {
    return runFormatter(
        Registry,
        {"sh", "-c",
         "(trap '' TERM; exec tail -f /dev/null </dev/null >/dev/null 2>&1) "
         "& child=$!; printf '%s' \"$child\" > \"$1\"; sleep 0.2; exit 0",
         "nixd-test", DescendantFile.path()},
        std::filesystem::current_path(), "input");
  });
  ExactProcessCleanup LeaderCleanup;
  const pid_t Leader = waitForLeader(Registry);
  LeaderCleanup.setLeader(Leader);
  if (Leader <= 0) {
    Registry.cancelAll();
    (void)Run.get();
    FAIL() << "formatter leader was never registered";
  }
  const pid_t Descendant = waitForPIDFile(DescendantFile);
  DescendantCleanup.setPID(Descendant);
  if (Descendant <= 0) {
    Registry.cancelAll();
    (void)Run.get();
    FAIL() << "formatter descendant PID was never published";
  }
  EXPECT_EQ(::getpgid(Descendant), Leader);

  const FormatterRunResult Result = Run.get();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;

  EXPECT_FALSE(Result.Cancelled);
  EXPECT_EQ(Result.ExitStatus, 0);
  EXPECT_LT(Elapsed, 600ms);
  ASSERT_TRUE(waitForPIDGone(Descendant));
  DescendantCleanup.release();
  expectReaped(Leader);
}

TEST(FormatterLifecycle, RepeatedOutcomesKeepFDsStableAndReapOnce) {
  const size_t Before = countFormatterDescriptors();
  for (unsigned Iteration = 0; Iteration < 4; ++Iteration) {
    FormatterProcessRegistry Registry;
    auto Normal = std::async(std::launch::async, [&] {
      return runFormatter(Registry, {"cat"}, std::filesystem::current_path(),
                          "normal");
    });
    ExactProcessCleanup NormalCleanup;
    const pid_t NormalPID = waitForLeader(Registry);
    NormalCleanup.setLeader(NormalPID);
    ASSERT_GT(NormalPID, 0);
    EXPECT_EQ(Normal.get().Stdout, "normal");
    expectReaped(NormalPID);

    auto Error = std::async(std::launch::async, [&] {
      return runFormatter(Registry, {"sh", "-c", "exit 7"},
                          std::filesystem::current_path(), "error");
    });
    ExactProcessCleanup ErrorCleanup;
    const pid_t ErrorPID = waitForLeader(Registry);
    ErrorCleanup.setLeader(ErrorPID);
    ASSERT_GT(ErrorPID, 0);
    EXPECT_NE(Error.get().ExitStatus, 0);
    expectReaped(ErrorPID);

    FormatterProcessRegistry CancelRegistry;
    auto Cancel = std::async(std::launch::async, [&] {
      return runFormatter(
          CancelRegistry, {"sh", "-c", "trap '' TERM; exec tail -f /dev/null"},
          std::filesystem::current_path(), std::string(1024 * 1024, 'x'));
    });
    ExactProcessCleanup CancelCleanup;
    const pid_t CancelPID = waitForLeader(CancelRegistry);
    CancelCleanup.setLeader(CancelPID);
    ASSERT_GT(CancelPID, 0);
    CancelRegistry.cancelAll();
    EXPECT_TRUE(Cancel.get().Cancelled);
    expectReaped(CancelPID);
  }
  EXPECT_EQ(countFormatterDescriptors(), Before);
}

TEST(FormatterLifecycle, ForkPipedParentEndpointsAreCloseOnExec) {
  int In = -1;
  int Out = -1;
  int Err = -1;
  pid_t ProcessGroup = -1;
  const pid_t Child = forkPiped(In, Out, Err, &ProcessGroup);
  if (Child == 0)
    _exit(0);
  ExactProcessCleanup Cleanup;
  Cleanup.setLeader(Child);
  const int InFlags = ::fcntl(In, F_GETFD);
  const int OutFlags = ::fcntl(Out, F_GETFD);
  const int ErrFlags = ::fcntl(Err, F_GETFD);
  (void)::close(In);
  (void)::close(Out);
  (void)::close(Err);
  int Status = 0;
  const pid_t Reaped = ::waitpid(Child, &Status, 0);

  EXPECT_GT(Child, 0);
  EXPECT_EQ(ProcessGroup, Child);
  EXPECT_NE(InFlags & FD_CLOEXEC, 0);
  EXPECT_NE(OutFlags & FD_CLOEXEC, 0);
  EXPECT_NE(ErrFlags & FD_CLOEXEC, 0);
  EXPECT_EQ(Reaped, Child);
}

TEST(FormatterLifecycle, OwnedDescriptorCloseIsNeverRetriedAfterEINTR) {
  util::AutoCloseFD FD(12345);
  unsigned Calls = 0;
  bool ReleasedBeforeClose = false;

  detail::closeOwnedWith(FD, [&](int RawFD) {
    ++Calls;
    EXPECT_EQ(RawFD, 12345);
    ReleasedBeforeClose = FD.isReleased();
    if (Calls == 1) {
      errno = EINTR;
      return -1;
    }
    return 0;
  });

  EXPECT_EQ(Calls, 1U);
  EXPECT_TRUE(ReleasedBeforeClose);
  EXPECT_TRUE(FD.isReleased());
}

TEST(FormatterLifecycle, ConcurrentFormatterExecDoesNotHoldEarlierStdinEOF) {
  TemporaryPIDFile FirstReady;
  FormatterProcessRegistry FirstRegistry;
  const std::string Input(2 * 1024 * 1024, 'x');
  auto First = std::async(std::launch::async, [&] {
    return runFormatter(FirstRegistry,
                        {"sh", "-c",
                         "printf '%s' \"$$\" > \"$1\"; sleep 0.2; exec cat",
                         "nixd-test", FirstReady.path()},
                        std::filesystem::current_path(), Input);
  });
  ExactProcessCleanup FirstCleanup;
  const pid_t FirstPID = waitForLeader(FirstRegistry);
  FirstCleanup.setLeader(FirstPID);
  if (FirstPID <= 0) {
    FirstRegistry.cancelAll();
    (void)First.get();
    FAIL() << "first formatter leader was never registered";
  }
  EXPECT_EQ(waitForPIDFile(FirstReady), FirstPID);

  FormatterProcessRegistry SecondRegistry;
  auto Second = std::async(std::launch::async, [&] {
    return runFormatter(SecondRegistry,
                        {"sh", "-c", "trap '' TERM; exec tail -f /dev/null"},
                        std::filesystem::current_path(), "");
  });
  ExactProcessCleanup SecondCleanup;
  const pid_t SecondPID = waitForLeader(SecondRegistry);
  SecondCleanup.setLeader(SecondPID);
  if (SecondPID <= 0) {
    SecondRegistry.cancelAll();
    FirstRegistry.cancelAll();
    (void)Second.get();
    (void)First.get();
    FAIL() << "second formatter leader was never registered";
  }

  const auto FirstStatus = First.wait_for(1500ms);
  SecondRegistry.cancelAll();
  if (FirstStatus != std::future_status::ready)
    FirstRegistry.cancelAll();
  const FormatterRunResult SecondResult = Second.get();
  const FormatterRunResult FirstResult = First.get();

  EXPECT_EQ(FirstStatus, std::future_status::ready);
  EXPECT_TRUE(SecondResult.Cancelled);
  if (FirstStatus == std::future_status::ready) {
    EXPECT_FALSE(FirstResult.Cancelled);
    EXPECT_EQ(FirstResult.Stdout, Input);
  }
  expectReaped(FirstPID);
  expectReaped(SecondPID);
}

TEST(FormatterLifecycle, NonExecEvaluatorChildDoesNotHoldFormatterStdinEOF) {
  TemporaryPIDFile FormatterReady;
  FormatterProcessRegistry Registry;
  const std::string Input(2 * 1024 * 1024, 'x');
  auto Formatter = std::async(std::launch::async, [&] {
    return runFormatter(Registry,
                        {"sh", "-c",
                         "printf '%s' \"$$\" > \"$1\"; sleep 0.2; exec cat",
                         "nixd-test", FormatterReady.path()},
                        std::filesystem::current_path(), Input);
  });
  ExactProcessCleanup FormatterCleanup;
  const pid_t FormatterPID = waitForLeader(Registry);
  FormatterCleanup.setLeader(FormatterPID);
  if (FormatterPID <= 0) {
    Registry.cancelAll();
    (void)Formatter.get();
    FAIL() << "formatter leader was never registered";
  }
  EXPECT_EQ(waitForPIDFile(FormatterReady), FormatterPID);

  int Ready[2];
  ASSERT_EQ(::pipe(Ready), 0);
  const std::array ChildFDs{Ready[1]};
  AttrSetClientProc Evaluator(
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
      {}, ChildFDs);
  (void)::close(Ready[1]);
  ExactProcessCleanup EvaluatorCleanup;
  EvaluatorCleanup.setLeader(Evaluator.pid());
  char Byte = 0;
  ssize_t Read;
  do {
    Read = ::read(Ready[0], &Byte, 1);
  } while (Read < 0 && errno == EINTR);
  (void)::close(Ready[0]);
  EXPECT_EQ(Read, 1);
  EXPECT_EQ(Byte, 'R');

  const auto FormatterStatus = Formatter.wait_for(1500ms);
  if (FormatterStatus != std::future_status::ready)
    Registry.cancelAll();
  EXPECT_TRUE(Evaluator.stop());
  const FormatterRunResult Result = Formatter.get();

  EXPECT_EQ(FormatterStatus, std::future_status::ready);
  if (FormatterStatus == std::future_status::ready) {
    EXPECT_FALSE(Result.Cancelled);
    EXPECT_EQ(Result.Stdout, Input);
  }
  expectReaped(FormatterPID);
  expectReaped(Evaluator.pid());
}

TEST(FormatterLifecycle, CancelsPoolSaturatingFormattersAsOneBatch) {
  std::atomic<unsigned> AliveTrees = 4;
  std::chrono::milliseconds GraceTotal(0);
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t, int Signal) {
            if (Signal == 0)
              return AliveTrees ? 0 : -1;
            if (Signal == SIGKILL)
              --AliveTrees;
            return 0;
          },
      .WaitForGrace =
          [&](std::chrono::milliseconds Grace) { GraceTotal += Grace; },
  };
  FormatterProcessRegistry Registry(Backend);
  std::vector<std::optional<FormatterProcess>> Processes;
  for (pid_t PID = 5001; PID <= 5004; ++PID)
    Processes.push_back(Registry.launch(
        [PID] { return util::PipedProc(PID, PID, -1, -1, -1); }));

  Registry.cancelAll();

  EXPECT_EQ(GraceTotal, 500ms);
  for (auto &Process : Processes) {
    ASSERT_TRUE(Process);
    EXPECT_TRUE(Process->Identity->cancellationRequested());
    EXPECT_TRUE(Process->Identity->markReaped());
    Registry.deregister(Process->Identity);
  }
}

TEST(FormatterLifecycle, FakeBackendEndsSharedGraceWhenEveryTreeExits) {
  bool Alive = true;
  std::chrono::milliseconds GraceTotal(0);
  std::vector<int> Signals;
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t, int Signal) {
            if (Signal == 0)
              return Alive ? 0 : -1;
            Signals.push_back(Signal);
            return 0;
          },
      .WaitForGrace =
          [&](std::chrono::milliseconds Grace) {
            GraceTotal += Grace;
            Alive = false;
          },
  };
  const std::array Trees{
      std::make_shared<ProcessTreeIdentity>(5101, 5101),
      std::make_shared<ProcessTreeIdentity>(5102, 5102),
  };

  cancelProcessTrees(Trees, Backend);

  EXPECT_EQ(Signals, (std::vector<int>{SIGTERM, SIGTERM}));
  EXPECT_LT(GraceTotal, 500ms);
  for (const auto &Tree : Trees)
    EXPECT_TRUE(Tree->markReaped());
}

TEST(FormatterLifecycle,
     CancellationKeepsSharedGraceForOwnedGroupAfterLeaderExit) {
  bool GroupAlive = true;
  std::chrono::milliseconds GraceTotal(0);
  std::vector<int> Signals;
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t Target, int Signal) {
            EXPECT_EQ(Target, -5201);
            if (Signal == 0) {
              if (GroupAlive)
                return 0;
              errno = ESRCH;
              return -1;
            }
            Signals.push_back(Signal);
            if (Signal == SIGKILL)
              GroupAlive = false;
            return 0;
          },
      .WaitForGrace =
          [&](std::chrono::milliseconds Delay) { GraceTotal += Delay; },
      .ObserveLeader = [](pid_t) { return 1; },
  };
  auto Identity = std::make_shared<ProcessTreeIdentity>(5201, 5201);
  const std::array Trees{Identity};

  cancelProcessTrees(Trees, Backend);

  EXPECT_EQ(GraceTotal, 500ms);
  EXPECT_EQ(Signals, (std::vector<int>{SIGTERM, SIGKILL}));
  EXPECT_TRUE(Identity->markReaped());
}

TEST(FormatterLifecycle, CompletionWaitsForWinningShutdownGraceBeforeSoleReap) {
  std::atomic<bool> Alive = true;
  std::atomic<int64_t> GraceMillis = 0;
  std::atomic<unsigned> ReapCalls = 0;
  std::atomic<bool> ReapedBeforeFullGrace = false;
  std::mutex GraceMutex;
  std::condition_variable GraceChanged;
  bool GraceEntered = false;
  bool AllowGrace = false;
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t Target, int Signal) {
            EXPECT_EQ(Target, -5301);
            if (Signal == 0) {
              if (Alive)
                return 0;
              errno = ESRCH;
              return -1;
            }
            if (Signal == SIGKILL)
              Alive = false;
            return 0;
          },
      .WaitForGrace =
          [&](std::chrono::milliseconds Delay) {
            GraceMillis += Delay.count();
            std::unique_lock Lock(GraceMutex);
            if (!GraceEntered) {
              GraceEntered = true;
              GraceChanged.notify_all();
              GraceChanged.wait(Lock, [&] { return AllowGrace; });
            }
          },
      .ObserveLeader = [](pid_t) { return 1; },
      .WaitPID =
          [&](pid_t PID, int *Status, int Options) {
            ++ReapCalls;
            EXPECT_EQ(PID, 5301);
            EXPECT_EQ(Options, 0);
            ReapedBeforeFullGrace = GraceMillis.load() < 500;
            *Status = 0;
            return PID;
          },
  };
  FormatterProcessRegistry Registry(Backend);
  auto Process =
      Registry.launch([] { return util::PipedProc(5301, 5301, -1, -1, -1); });
  ASSERT_TRUE(Process);

  auto Cancellation = std::async(std::launch::async,
                                 [&] { Registry.cancel(Process->Identity); });
  bool Entered = false;
  {
    std::unique_lock Lock(GraceMutex);
    Entered = GraceChanged.wait_for(Lock, 2s, [&] { return GraceEntered; });
  }
  if (!Entered) {
    {
      std::lock_guard Guard(GraceMutex);
      AllowGrace = true;
    }
    GraceChanged.notify_all();
    Cancellation.get();
    Registry.deregister(Process->Identity);
    FAIL() << "shutdown cancellation never entered its grace interval";
  }

  auto Completion = std::async(std::launch::async, [&] {
    const bool CompletionWon =
        Registry.terminateCompletedOwnedGroup(Process->Identity);
    int Status = 0;
    return std::pair(CompletionWon,
                     Registry.reap(Process->Identity, Status, 0));
  });
  const auto EarlyCompletion = Completion.wait_for(100ms);

  {
    std::lock_guard Guard(GraceMutex);
    AllowGrace = true;
  }
  GraceChanged.notify_all();
  Cancellation.get();
  const auto [CompletionWon, Reaped] = Completion.get();
  Registry.deregister(Process->Identity);

  EXPECT_EQ(EarlyCompletion, std::future_status::timeout);
  EXPECT_FALSE(CompletionWon);
  EXPECT_FALSE(ReapedBeforeFullGrace);
  EXPECT_EQ(GraceMillis, 500);
  EXPECT_EQ(ReapCalls, 1U);
  EXPECT_EQ(Reaped, 5301);
}

TEST(FormatterLifecycle,
     ControllerCancelsPoolSaturationBeforeProviderStrandRetirement) {
  Controller C(std::make_unique<lspserver::InboundPort>(-1),
               std::make_unique<lspserver::OutboundPort>());
  TemporaryPIDFile TemporaryFile;
  const std::filesystem::path File = TemporaryFile.path();
  ControllerTestPeer::prepareFormatting(C, File, std::string(1024 * 1024, 'x'));

  std::atomic<bool> CancelObservedBeforeRetirement = false;
  ControllerTestPeer::installProviders(C, [&](const ProviderKey &,
                                              const std::filesystem::path &,
                                              ProviderWorker::DeathCallback) {
    return std::make_shared<OrderingWorker>(
        [&] { return ControllerTestPeer::allFormattersCancelled(C); },
        CancelObservedBeforeRetirement);
  });
  Configuration Config = defaultConfiguration();
  Config.nixpkgs.expr = "{ }";
  Config.formatting.command = {"sh", "-c",
                               "trap '' TERM; exec tail -f /dev/null"};
  std::binary_semaphore Applied(0);
  ControllerTestPeer::apply(C, std::move(Config),
                            [&](ProviderApplyResult Result) {
                              EXPECT_EQ(Result, ProviderApplyResult::Ready);
                              Applied.release();
                            });
  ASSERT_TRUE(Applied.try_acquire_for(2s));

  const size_t PoolSize = ControllerTestPeer::poolSize(C);
  ASSERT_GT(PoolSize, 0U);
  std::atomic<size_t> Replies = 0;
  for (size_t Index = 0; Index < PoolSize; ++Index) {
    ControllerTestPeer::format(
        C, File, [&](llvm::Expected<std::vector<lspserver::TextEdit>> Result) {
          if (!Result)
            llvm::consumeError(Result.takeError());
          ++Replies;
        });
  }

  const std::vector<pid_t> PIDs = waitForControllerFormatters(C, PoolSize);
  std::future<void> Shutdown;
  std::vector<std::unique_ptr<ExactProcessCleanup>> Cleanups;
  std::vector<std::unique_ptr<ExactProcessWatchdog>> Watchdogs;
  for (pid_t PID : PIDs) {
    auto Cleanup = std::make_unique<ExactProcessCleanup>();
    Cleanup->setLeader(PID);
    Cleanups.push_back(std::move(Cleanup));
    Watchdogs.push_back(std::make_unique<ExactProcessWatchdog>(PID));
  }
  ASSERT_EQ(PIDs.size(), PoolSize);

  Shutdown =
      std::async(std::launch::async, [&] { ControllerTestPeer::shutdown(C); });
  ASSERT_EQ(Shutdown.wait_for(3s), std::future_status::ready);
  Shutdown.get();
  for (auto &Watchdog : Watchdogs)
    Watchdog->complete();

  EXPECT_TRUE(CancelObservedBeforeRetirement);
  EXPECT_EQ(Replies.load(), PoolSize);
  for (const auto &Watchdog : Watchdogs)
    EXPECT_FALSE(Watchdog->fired());
  for (pid_t PID : PIDs)
    expectReaped(PID);
}

TEST(FormatterLifecycle, EmptyCommandRepliesNoEditsWithoutLaunchAttempt) {
  Controller C(std::make_unique<lspserver::InboundPort>(-1),
               std::make_unique<lspserver::OutboundPort>());
  TemporaryPIDFile TemporaryFile;
  const std::filesystem::path File = TemporaryFile.path();
  ControllerTestPeer::prepareFormatting(C, File, "{ value = 1; }\n");
  ControllerTestPeer::setFormattingCommand(C, {});
  const size_t Attempts = ControllerTestPeer::formattingLaunchAttempts(C);
  std::promise<llvm::Expected<std::vector<lspserver::TextEdit>>> Reply;
  auto Result = Reply.get_future();

  ControllerTestPeer::format(
      C, File,
      [&](llvm::Expected<std::vector<lspserver::TextEdit>> Edits) mutable {
        Reply.set_value(std::move(Edits));
      });

  ASSERT_EQ(Result.wait_for(2s), std::future_status::ready);
  auto Edits = Result.get();
  if (!Edits)
    FAIL() << llvm::toString(Edits.takeError());
  EXPECT_TRUE(Edits->empty());
  EXPECT_EQ(ControllerTestPeer::formattingLaunchAttempts(C), Attempts);
}

TEST(FormatterLifecycle, FakeBackendUsesOneGraceThenKill) {
  bool Alive = true;
  std::chrono::milliseconds GraceTotal(0);
  std::vector<int> Signals;
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t Target, int Signal) {
            EXPECT_EQ(Target, -6001);
            if (Signal == 0)
              return Alive ? 0 : -1;
            Signals.push_back(Signal);
            if (Signal == SIGKILL)
              Alive = false;
            return 0;
          },
      .WaitForGrace =
          [&](std::chrono::milliseconds Grace) { GraceTotal += Grace; },
  };
  auto Identity = std::make_shared<ProcessTreeIdentity>(6001, 6001);

  const std::array Trees{Identity};
  cancelProcessTrees(Trees, Backend);

  EXPECT_EQ(Signals, (std::vector<int>{SIGTERM, SIGKILL}));
  EXPECT_EQ(GraceTotal, 500ms);
  EXPECT_TRUE(Identity->markReaped());
  EXPECT_FALSE(Identity->markReaped());
  EXPECT_FALSE(Identity->signalIfAlive(SIGKILL, Backend));
}

TEST(FormatterLifecycle, RegistryReapInvokesWaitPIDExactlyOnce) {
  bool Alive = true;
  unsigned WaitCalls = 0;
  ProcessTreeBackend Backend{
      .Kill =
          [&](pid_t Target, int Signal) {
            EXPECT_EQ(Target, -6101);
            if (Signal == 0) {
              if (Alive)
                return 0;
              errno = ESRCH;
              return -1;
            }
            if (Signal == SIGKILL)
              Alive = false;
            return 0;
          },
      .WaitForGrace = [](std::chrono::milliseconds) {},
      .WaitPID =
          [&](pid_t PID, int *Status, int Options) {
            ++WaitCalls;
            EXPECT_EQ(PID, 6101);
            EXPECT_EQ(Options, 0);
            *Status = 42;
            return PID;
          },
  };
  FormatterProcessRegistry Registry(Backend);
  auto Process =
      Registry.launch([] { return util::PipedProc(6101, 6101, -1, -1, -1); });
  ASSERT_TRUE(Process);
  Registry.cancel(Process->Identity);
  int Status = 0;

  EXPECT_EQ(Registry.reap(Process->Identity, Status, 0), 6101);
  errno = 0;
  EXPECT_EQ(Registry.reap(Process->Identity, Status, 0), -1);
  EXPECT_EQ(errno, ECHILD);
  EXPECT_EQ(WaitCalls, 1U);
  EXPECT_EQ(Status, 42);
  Registry.deregister(Process->Identity);
}

} // namespace
} // namespace nixd
