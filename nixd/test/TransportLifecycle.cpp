#include "../lib/Support/ForkPipedInternal.h"
#include "lspserver/LSPServer.h"
#include "nixd/Eval/AttrSetClient.h"
#include "nixd/Support/ForkPiped.h"

#include <gtest/gtest.h>

#include <barrier>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace nixd {
namespace {

size_t countOpenDescriptors() {
  size_t Count = 0;
  for (int FD = 0; FD < ::getdtablesize(); ++FD) {
    errno = 0;
    if (::fcntl(FD, F_GETFD) != -1 || errno != EBADF)
      ++Count;
  }
  return Count;
}

std::string readAll(int FD) {
  std::string Result;
  char Buffer[64];
  for (;;) {
    const ssize_t Read = ::read(FD, Buffer, sizeof(Buffer));
    if (Read > 0) {
      Result.append(Buffer, static_cast<size_t>(Read));
      continue;
    }
    if (Read < 0 && errno == EINTR)
      continue;
    return Result;
  }
}

class PendingCallServer final : public lspserver::LSPServer {
public:
  PendingCallServer(std::unique_ptr<lspserver::InboundPort> In,
                    std::unique_ptr<lspserver::OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {}

  void request(lspserver::Callback<llvm::json::Value> Reply) {
    auto Request =
        mkOutMethod<llvm::json::Value, llvm::json::Value>("test/request");
    Request(nullptr, std::move(Reply));
  }

  void requestOn(lspserver::OutboundPort &Out,
                 lspserver::Callback<llvm::json::Value> Reply) {
    auto Request =
        mkOutMethod<llvm::json::Value, llvm::json::Value>("test/request", &Out);
    Request(nullptr, std::move(Reply));
  }
};

TEST(TransportLifecycle, ExplicitCloseFailsPendingCallsExactlyOnce) {
  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  PendingCallServer Server(std::make_unique<lspserver::InboundPort>(-1),
                           std::make_unique<lspserver::OutboundPort>(Stream));
  unsigned Calls = 0;
  std::string Failure;
  Server.request([&](llvm::Expected<llvm::json::Value> Result) {
    ++Calls;
    ASSERT_FALSE(Result);
    Failure = llvm::toString(Result.takeError());
  });

  Server.closeInbound();
  Server.closeInbound();

  EXPECT_EQ(Calls, 1);
  EXPECT_NE(Failure.find("closed"), std::string::npos);
}

TEST(TransportLifecycle, EndOfInputFailsPendingCalls) {
  int Pipe[2];
  ASSERT_EQ(::pipe(Pipe), 0);
  ASSERT_EQ(::close(Pipe[1]), 0);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  PendingCallServer Server(std::make_unique<lspserver::InboundPort>(Pipe[0]),
                           std::make_unique<lspserver::OutboundPort>(Stream));
  unsigned Calls = 0;
  Server.request([&](llvm::Expected<llvm::json::Value> Result) {
    ++Calls;
    ASSERT_FALSE(Result);
    llvm::consumeError(Result.takeError());
  });

  Server.run();

  EXPECT_EQ(Calls, 1);
  EXPECT_EQ(::close(Pipe[0]), 0);
}

TEST(TransportLifecycle, CloseRejectsConcurrentLaterCallsWithoutWriting) {
  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  PendingCallServer Server(std::make_unique<lspserver::InboundPort>(-1),
                           std::make_unique<lspserver::OutboundPort>(Stream));
  Server.closeInbound();

  static constexpr unsigned Threads = 8;
  std::barrier Start(Threads);
  std::atomic<unsigned> Rejected = 0;
  std::vector<std::thread> Callers;
  for (unsigned I = 0; I < Threads; ++I) {
    Callers.emplace_back([&] {
      Start.arrive_and_wait();
      Server.request([&](llvm::Expected<llvm::json::Value> Result) {
        EXPECT_FALSE(Result);
        if (!Result)
          llvm::consumeError(Result.takeError());
        ++Rejected;
      });
    });
  }
  for (auto &Caller : Callers)
    Caller.join();
  Stream.flush();

  EXPECT_EQ(Rejected, Threads);
  EXPECT_TRUE(Output.empty());
}

TEST(TransportLifecycle, CapacityEvictionCallbackCanCloseAndBindReentrantly) {
  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  PendingCallServer Server(std::make_unique<lspserver::InboundPort>(-1),
                           std::make_unique<lspserver::OutboundPort>(Stream));
  bool Evicted = false;
  bool ReentrantRejected = false;
  Server.request([&](llvm::Expected<llvm::json::Value> Result) {
    EXPECT_FALSE(Result);
    if (!Result)
      llvm::consumeError(Result.takeError());
    Evicted = true;
    Server.closeInbound();
    Server.request([&](llvm::Expected<llvm::json::Value> Nested) {
      EXPECT_FALSE(Nested);
      if (!Nested)
        llvm::consumeError(Nested.takeError());
      ReentrantRejected = true;
    });
  });

  for (unsigned I = 0; I < 100; ++I) {
    Server.request([](llvm::Expected<llvm::json::Value> Result) {
      EXPECT_FALSE(Result);
      if (!Result)
        llvm::consumeError(Result.takeError());
    });
  }

  EXPECT_TRUE(Evicted);
  EXPECT_TRUE(ReentrantRejected);
}

TEST(TransportLifecycle, DeadChildIsObservableReapedAndStopsIdempotently) {
  std::mutex Mutex;
  std::condition_variable Died;
  bool SawDeath = false;
  std::thread::id DeathThread;
  const auto CallerThread = std::this_thread::get_id();
  pid_t PID = -1;

  {
    AttrSetClientProc Process([] { return 0; },
                              [&] {
                                {
                                  std::lock_guard Guard(Mutex);
                                  SawDeath = true;
                                  DeathThread = std::this_thread::get_id();
                                }
                                Died.notify_one();
                              });
    PID = Process.pid();
    {
      std::unique_lock Lock(Mutex);
      ASSERT_TRUE(Died.wait_for(Lock, std::chrono::seconds(2),
                                [&] { return SawDeath; }));
    }
    EXPECT_NE(DeathThread, CallerThread);
    EXPECT_FALSE(Process.alive());
    Process.stop();
    Process.stop();
  }

  int Status = 0;
  errno = 0;
  EXPECT_EQ(::waitpid(PID, &Status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
}

TEST(TransportLifecycle, StopEscalatesAndReapsChildIgnoringTermination) {
  int Ready[2];
  ASSERT_EQ(::pipe(Ready), 0);
  pid_t PID = -1;
  const auto Start = std::chrono::steady_clock::now();
  {
    AttrSetClientProc Process([&] {
      ::signal(SIGTERM, SIG_IGN);
      const char Byte = 'x';
      if (::write(Ready[1], &Byte, 1) != 1)
        return 2;
      for (;;)
        ::pause();
    });
    PID = Process.pid();
    ASSERT_EQ(::close(Ready[1]), 0);
    char Byte = 0;
    ASSERT_EQ(::read(Ready[0], &Byte, 1), 1);
    EXPECT_EQ(Byte, 'x');
    Process.stop();
  }
  const auto Elapsed = std::chrono::steady_clock::now() - Start;
  EXPECT_LT(Elapsed, std::chrono::seconds(3));
  ASSERT_EQ(::close(Ready[0]), 0);

  int Status = 0;
  errno = 0;
  EXPECT_EQ(::waitpid(PID, &Status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
}

TEST(TransportLifecycle, ConcurrentStopWaitsForSingleCompletedStop) {
  int Ready[2];
  ASSERT_EQ(::pipe(Ready), 0);
  std::mutex Mutex;
  std::condition_variable Changed;
  bool PendingFailed = false;
  bool SawDeath = false;

  AttrSetClientProc Process(
      [&] {
        ::signal(SIGTERM, SIG_IGN);
        const char Byte = 'x';
        if (::write(Ready[1], &Byte, 1) != 1)
          return 2;
        for (;;)
          ::pause();
      },
      [&] {
        {
          std::lock_guard Guard(Mutex);
          SawDeath = true;
        }
        Changed.notify_all();
      });
  ASSERT_EQ(::close(Ready[1]), 0);
  char Byte = 0;
  ASSERT_EQ(::read(Ready[0], &Byte, 1), 1);
  ASSERT_EQ(::close(Ready[0]), 0);
  ASSERT_TRUE(Process.client());
  Process.client()->evalExpr("ignored",
                             [&](llvm::Expected<EvalExprResponse> Result) {
                               EXPECT_FALSE(Result);
                               if (!Result)
                                 llvm::consumeError(Result.takeError());
                               {
                                 std::lock_guard Guard(Mutex);
                                 PendingFailed = true;
                               }
                               Changed.notify_all();
                             });

  std::thread First([&] { EXPECT_TRUE(Process.stop()); });
  {
    std::unique_lock Lock(Mutex);
    ASSERT_TRUE(Changed.wait_for(Lock, std::chrono::seconds(2),
                                 [&] { return PendingFailed; }));
    ASSERT_FALSE(SawDeath);
  }

  EXPECT_TRUE(Process.stop());
  {
    std::lock_guard Guard(Mutex);
    EXPECT_TRUE(SawDeath);
  }
  First.join();
}

TEST(TransportLifecycle, InputThreadStopDefersJoinToOwningThread) {
  int Go[2];
  ASSERT_EQ(::pipe(Go), 0);
  std::mutex Mutex;
  std::condition_variable Died;
  bool SawDeath = false;
  bool InputStopCompleted = true;
  std::unique_ptr<AttrSetClientProc> Process;

  Process = std::make_unique<AttrSetClientProc>(
      [&] {
        char Byte = 0;
        if (::read(Go[0], &Byte, 1) != 1)
          return 2;
        return 0;
      },
      [&] {
        InputStopCompleted = Process->stop();
        {
          std::lock_guard Guard(Mutex);
          SawDeath = true;
        }
        Died.notify_one();
      });
  ASSERT_EQ(::close(Go[0]), 0);
  const char Byte = 'x';
  ASSERT_EQ(::write(Go[1], &Byte, 1), 1);
  ASSERT_EQ(::close(Go[1]), 0);
  {
    std::unique_lock Lock(Mutex);
    ASSERT_TRUE(
        Died.wait_for(Lock, std::chrono::seconds(2), [&] { return SawDeath; }));
  }

  EXPECT_FALSE(InputStopCompleted);
  EXPECT_TRUE(Process->stop());
}

TEST(TransportLifecycle,
     BrokenPipeDoesNotSignalParentAndFailsOnlyMatchingPendingCall) {
  const pid_t Child = ::fork();
  ASSERT_GE(Child, 0);
  if (Child == 0) {
    ::signal(SIGPIPE, SIG_DFL);
    int Pipe[2];
    if (::pipe(Pipe) != 0 || ::close(Pipe[0]) != 0)
      _exit(2);

    std::string GoodOutput;
    llvm::raw_string_ostream GoodStream(GoodOutput);
    PendingCallServer Server(
        std::make_unique<lspserver::InboundPort>(-1),
        std::make_unique<lspserver::OutboundPort>(GoodStream));
    llvm::raw_fd_ostream BrokenStream(Pipe[1], false);
    lspserver::OutboundPort BrokenOut(BrokenStream);
    unsigned GoodCalls = 0;
    unsigned BrokenCalls = 0;
    Server.request([&](llvm::Expected<llvm::json::Value> Result) {
      ++GoodCalls;
      if (!Result)
        llvm::consumeError(Result.takeError());
    });
    Server.requestOn(BrokenOut, [&](llvm::Expected<llvm::json::Value> Result) {
      if (!Result) {
        llvm::consumeError(Result.takeError());
        ++BrokenCalls;
      }
    });
    if (GoodCalls != 0 || BrokenCalls != 1)
      _exit(3);
    Server.closeInbound();
    if (GoodCalls != 1 || BrokenCalls != 1)
      _exit(4);
    BrokenStream.clear_error();
    ::close(Pipe[1]);
    _exit(0);
  }

  int Status = 0;
  ASSERT_EQ(::waitpid(Child, &Status, 0), Child);
  ASSERT_TRUE(WIFEXITED(Status))
      << "isolated dispatch terminated by signal " << WTERMSIG(Status);
  EXPECT_EQ(WEXITSTATUS(Status), 0);
}

TEST(TransportLifecycle, StopBoundsHostileProcessTreeAndKillsInheritedWriter) {
  int DescendantPipe[2];
  ASSERT_EQ(::pipe(DescendantPipe), 0);
  pid_t Descendant = -1;
  pid_t Leader = -1;
  const pid_t ParentGroup = ::getpgrp();
  const auto Start = std::chrono::steady_clock::now();
  {
    AttrSetClientProc Process([&] {
      ::signal(SIGTERM, SIG_IGN);
      const pid_t Child = ::fork();
      if (Child < 0)
        return 2;
      if (Child == 0) {
        ::signal(SIGTERM, SIG_IGN);
        for (;;)
          ::pause();
      }
      if (::write(DescendantPipe[1], &Child, sizeof(Child)) != sizeof(Child))
        return 3;
      for (;;)
        ::pause();
    });
    ASSERT_EQ(::close(DescendantPipe[1]), 0);
    ASSERT_EQ(::read(DescendantPipe[0], &Descendant, sizeof(Descendant)),
              sizeof(Descendant));
    ASSERT_GT(Descendant, 0);
    Leader = Process.pid();
    Process.stop();
  }
  const auto Elapsed = std::chrono::steady_clock::now() - Start;
  EXPECT_LT(Elapsed, std::chrono::seconds(3));
  ASSERT_EQ(::close(DescendantPipe[0]), 0);

  bool DescendantGone = false;
  const auto GoneDeadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  do {
    errno = 0;
    DescendantGone = ::kill(Descendant, 0) < 0 && errno == ESRCH;
    if (!DescendantGone)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while (!DescendantGone && std::chrono::steady_clock::now() < GoneDeadline);
  if (!DescendantGone)
    ::kill(Descendant, SIGKILL);
  EXPECT_TRUE(DescendantGone);
  int Status = 0;
  errno = 0;
  EXPECT_EQ(::waitpid(Leader, &Status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
  EXPECT_EQ(::getpgrp(), ParentGroup);
  EXPECT_EQ(::kill(-ParentGroup, 0), 0);
}

TEST(TransportLifecycle, AliveObservationLeavesExitedLeaderWaitableUntilStop) {
  int DescendantPipe[2];
  ASSERT_EQ(::pipe(DescendantPipe), 0);
  pid_t Descendant = -1;
  pid_t Leader = -1;
  {
    AttrSetClientProc Process([&] {
      const pid_t Child = ::fork();
      if (Child < 0)
        return 2;
      if (Child == 0) {
        for (;;)
          ::pause();
      }
      if (::write(DescendantPipe[1], &Child, sizeof(Child)) != sizeof(Child))
        return 3;
      return 0;
    });
    ASSERT_EQ(::close(DescendantPipe[1]), 0);
    ASSERT_EQ(::read(DescendantPipe[0], &Descendant, sizeof(Descendant)),
              sizeof(Descendant));
    Leader = Process.pid();

    const auto Deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (Process.alive() && std::chrono::steady_clock::now() < Deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_FALSE(Process.alive());

    siginfo_t Info{};
    errno = 0;
    EXPECT_EQ(::waitid(P_PID, Leader, &Info, WEXITED | WNOHANG | WNOWAIT), 0);
    EXPECT_EQ(Info.si_pid, Leader);
    Process.stop();
  }
  ASSERT_EQ(::close(DescendantPipe[0]), 0);
  int Status = 0;
  errno = 0;
  EXPECT_EQ(::waitpid(Leader, &Status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
  errno = 0;
  EXPECT_EQ(::kill(Descendant, 0), -1);
  EXPECT_EQ(errno, ESRCH);
}

TEST(TransportLifecycle,
     FastExitedLeaderRetainsIdentityThroughInheritedWriterShutdown) {
  int DescendantPipe[2];
  ASSERT_EQ(::pipe(DescendantPipe), 0);
  pid_t Descendant = -1;
  {
    AttrSetClientProc Process([&] {
      const pid_t Child = ::fork();
      if (Child < 0)
        return 2;
      if (Child == 0) {
        ::signal(SIGTERM, SIG_IGN);
        for (;;)
          ::pause();
      }
      if (::write(DescendantPipe[1], &Child, sizeof(Child)) != sizeof(Child))
        return 3;
      return 0;
    });
    ASSERT_EQ(::close(DescendantPipe[1]), 0);
    ASSERT_EQ(::read(DescendantPipe[0], &Descendant, sizeof(Descendant)),
              sizeof(Descendant));

    const auto Deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (Process.alive() && std::chrono::steady_clock::now() < Deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_FALSE(Process.alive());
    siginfo_t Info{};
    EXPECT_EQ(
        ::waitid(P_PID, Process.pid(), &Info, WEXITED | WNOHANG | WNOWAIT), 0);
    EXPECT_EQ(Info.si_pid, Process.pid());

    const auto Start = std::chrono::steady_clock::now();
    Process.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - Start,
              std::chrono::seconds(3));
  }
  ASSERT_EQ(::close(DescendantPipe[0]), 0);
  errno = 0;
  EXPECT_EQ(::kill(Descendant, 0), -1);
  EXPECT_EQ(errno, ESRCH);
}

TEST(TransportLifecycle,
     ForkPipedRoutesDistinctStreamsWithoutDescriptorGrowth) {
  const size_t Before = countOpenDescriptors();
  bool AllDistinct = true;
  bool AllRouted = true;
  for (unsigned Iteration = 0; Iteration < 12; ++Iteration) {
    int In = -1;
    int Out = -1;
    int Err = -1;
    const pid_t Child = forkPiped(In, Out, Err);
    if (Child == 0) {
      static constexpr char Stdout[] = "stdout";
      static constexpr char Stderr[] = "stderr";
      if (::write(STDOUT_FILENO, Stdout, sizeof(Stdout) - 1) < 0 ||
          ::write(STDERR_FILENO, Stderr, sizeof(Stderr) - 1) < 0)
        _exit(2);
      _exit(0);
    }

    AllDistinct = AllDistinct && Out != Err;
    ASSERT_EQ(::close(In), 0);
    const std::string Stdout = readAll(Out);
    const std::string Stderr = readAll(Err);
    AllRouted = AllRouted && Stdout == "stdout" && Stderr == "stderr";
    ASSERT_EQ(::close(Out), 0);
    if (Err != Out)
      ASSERT_EQ(::close(Err), 0);
    int Status = 0;
    ASSERT_EQ(::waitpid(Child, &Status, 0), Child);
    ASSERT_TRUE(WIFEXITED(Status));
    ASSERT_EQ(WEXITSTATUS(Status), 0);
  }

  EXPECT_TRUE(AllDistinct);
  EXPECT_TRUE(AllRouted);
  EXPECT_EQ(countOpenDescriptors(), Before);
}

TEST(TransportLifecycle, ForkPipedFailuresReleaseEveryOpenedDescriptor) {
  enum class FailurePoint { Pipe2, Pipe3, Fork };
  for (FailurePoint Failure :
       {FailurePoint::Pipe2, FailurePoint::Pipe3, FailurePoint::Fork}) {
    const size_t Before = countOpenDescriptors();
    for (unsigned Iteration = 0; Iteration < 12; ++Iteration) {
      unsigned PipeCalls = 0;
      detail::ForkPipedSyscalls Syscalls{
          .Pipe =
              [&](int *FDs) {
                ++PipeCalls;
                if ((Failure == FailurePoint::Pipe2 && PipeCalls == 2) ||
                    (Failure == FailurePoint::Pipe3 && PipeCalls == 3)) {
                  errno = EMFILE;
                  return -1;
                }
                return ::pipe(FDs);
              },
          .Fork =
              [&] {
                if (Failure == FailurePoint::Fork) {
                  errno = EAGAIN;
                  return pid_t{-1};
                }
                return ::fork();
              },
      };
      int In = -1;
      int Out = -1;
      int Err = -1;
      pid_t ProcessGroup = 4242;
      EXPECT_THROW(detail::forkPipedWith(In, Out, Err, &ProcessGroup, Syscalls),
                   std::system_error);
      EXPECT_EQ(In, -1);
      EXPECT_EQ(Out, -1);
      EXPECT_EQ(Err, -1);
      EXPECT_EQ(ProcessGroup, 4242);
    }
    EXPECT_EQ(countOpenDescriptors(), Before);
  }
}

} // namespace
} // namespace nixd
