#include "nixd/Support/ForkPiped.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

const char *Self;

// Kill and reap even when an assertion fails, so regressions cannot hang tests.
struct Child {
  pid_t PID = -1;
  int In = -1, Out = -1, Err = -1;

  ~Child() {
    for (int FD : {In, Out, Err})
      if (FD != -1)
        close(FD);
    if (PID > 0) {
      kill(PID, SIGKILL);
      while (waitpid(PID, nullptr, 0) == -1 && errno == EINTR) {
      }
    }
  }

  void start(const std::vector<std::string> &Args) {
    std::vector<char *> Argv{const_cast<char *>(Self)};
    for (const auto &Arg : Args)
      Argv.push_back(const_cast<char *>(Arg.c_str()));
    Argv.push_back(nullptr);
    PID = nixd::forkPiped(In, Out, Err);
    if (PID == 0) {
      execv(Self, Argv.data());
      _exit(127);
    }
  }

  void closeInput() {
    close(In);
    In = -1;
  }

  int finish() {
    int Status;
    pid_t Result;
    do {
      Result = waitpid(PID, &Status, 0);
    } while (Result == -1 && errno == EINTR);
    if (Result != PID)
      return -1;
    PID = -1;
    return WIFEXITED(Status) ? WEXITSTATUS(Status) : -1;
  }
};

// A bounded read also distinguishes EOF (0) from a timeout/error (-1).
ssize_t readReady(int FD, char &Byte) {
  pollfd Poll{FD, POLLIN, 0};
  int Result;
  do {
    Result = poll(&Poll, 1, 2000);
  } while (Result == -1 && errno == EINTR);
  if (Result <= 0)
    return -1;
  return read(FD, &Byte, 1);
}

TEST(ForkPiped, StdioSurvivesExec) {
  Child Proc;
  Proc.start({"--stdio"});
  EXPECT_NE(Proc.Out, Proc.Err);
  for (int FD : {Proc.In, Proc.Out, Proc.Err}) {
    int Flags = fcntl(FD, F_GETFD);
    ASSERT_NE(Flags, -1);
    EXPECT_NE(Flags & FD_CLOEXEC, 0);
  }
  ASSERT_EQ(write(Proc.In, "x", 1), 1);
  Proc.closeInput();
  char Byte;
  ASSERT_EQ(readReady(Proc.Out, Byte), 1);
  EXPECT_EQ(Byte, 'x');
  ASSERT_EQ(readReady(Proc.Err, Byte), 1);
  EXPECT_EQ(Byte, 'e');
  ASSERT_EQ(readReady(Proc.Out, Byte), 0);
  ASSERT_EQ(readReady(Proc.Err, Byte), 0);
  EXPECT_EQ(Proc.finish(), 0);
}

TEST(ForkPiped, UnrelatedWorkerDoesNotDelayEOF) {
  Child First;
  First.start({"--drain"});
  char Byte;
  ASSERT_EQ(readReady(First.Out, Byte), 1);
  ASSERT_EQ(Byte, 'r');

  Child Second;
  Second.start({"--drain"});
  ASSERT_EQ(readReady(Second.Out, Byte), 1);
  ASSERT_EQ(Byte, 'r'); // Second has exec'd and is waiting for input.

  First.closeInput();
  ASSERT_EQ(readReady(First.Out, Byte), 0);
  EXPECT_EQ(First.finish(), 0);
  EXPECT_EQ(waitpid(Second.PID, nullptr, WNOHANG), 0);

  Second.closeInput();
  ASSERT_EQ(readReady(Second.Out, Byte), 0);
  EXPECT_EQ(Second.finish(), 0);
}

TEST(ForkPiped, UnrelatedDescriptorsCloseOnExec) {
  Child First;
  First.start({"--drain"});
  char Byte;
  ASSERT_EQ(readReady(First.Out, Byte), 1);
  ASSERT_EQ(Byte, 'r');

  // Check all three parent endpoints across exec, including FIFO identity:
  // a reused descriptor number alone does not imply a leak.
  std::vector<std::string> Args{"--probe"};
  for (int FD : {First.In, First.Out, First.Err}) {
    struct stat St{};
    ASSERT_EQ(fstat(FD, &St), 0);
    Args.push_back(std::to_string(FD));
    Args.push_back(std::to_string(St.st_dev));
    Args.push_back(std::to_string(St.st_ino));
  }
  Child Second;
  Second.start(Args);
  ASSERT_EQ(readReady(Second.Out, Byte), 1);
  ASSERT_EQ(Byte, 'r'); // Second has exec'd, checked the FDs, and is waiting.

  Second.closeInput();
  ASSERT_EQ(readReady(Second.Out, Byte), 0);
  EXPECT_EQ(Second.finish(), 0);
  First.closeInput();
  ASSERT_EQ(readReady(First.Out, Byte), 0);
  EXPECT_EQ(First.finish(), 0);
}

TEST(ForkPiped, ClosedStandardDescriptors) {
  // Isolate closing stdio from the test runner. Exercise every nonempty subset.
  for (int Mask = 1; Mask < 8; ++Mask) {
    Child Proc;
    Proc.start({"--closed-stdio", std::to_string(Mask)});
    char Byte;
    ASSERT_EQ(readReady(Proc.Out, Byte), 0);
    EXPECT_EQ(Proc.finish(), 0) << "closed descriptor mask: " << Mask;
  }
}

TEST(ForkPiped, ConcurrentStarts) {
  std::vector<std::thread> Threads;
  for (int I = 0; I < 4; ++I)
    Threads.emplace_back([] {
      for (int J = 0; J < 8; ++J) {
        Child Proc;
        Proc.start({"--drain"});
        char Byte;
        ASSERT_EQ(readReady(Proc.Out, Byte), 1);
        ASSERT_EQ(Byte, 'r');
        Proc.closeInput();
        ASSERT_EQ(readReady(Proc.Out, Byte), 0);
        EXPECT_EQ(Proc.finish(), 0);
      }
    });
  for (auto &Thread : Threads)
    Thread.join();
}

int stdioWorker() {
  char Byte;
  if (read(STDIN_FILENO, &Byte, 1) != 1 || Byte != 'x')
    return 1;
  if (write(STDOUT_FILENO, &Byte, 1) != 1 || write(STDERR_FILENO, "e", 1) != 1)
    return 2;
  return 0;
}

int drainWorker() {
  if (write(STDOUT_FILENO, "r", 1) != 1)
    return 1;
  char Byte;
  ssize_t Result;
  do {
    Result = read(STDIN_FILENO, &Byte, 1);
  } while (Result > 0 || (Result == -1 && errno == EINTR));
  return Result == 0 ? 0 : 2;
}

} // namespace

int main(int Argc, char **Argv) {
  Self = Argv[0];
  // Bound waits even if a child closes stdout but fails to exit.
  alarm(20);
  if (Argc >= 2) {
    const std::string Mode = Argv[1];
    if (Mode == "--stdio")
      return stdioWorker();
    if (Mode == "--drain")
      return drainWorker();
    if (Mode == "--probe" && Argc == 11) {
      for (int I = 2; I < Argc; I += 3) {
        struct stat St{};
        if (fstat(std::atoi(Argv[I]), &St) == 0 && S_ISFIFO(St.st_mode) &&
            St.st_dev == std::strtoull(Argv[I + 1], nullptr, 10) &&
            St.st_ino == std::strtoull(Argv[I + 2], nullptr, 10))
          return 3;
      }
      return drainWorker();
    }
    if (Mode == "--closed-stdio" && Argc == 3) {
      const int Mask = std::atoi(Argv[2]);
      for (int FD = 0; FD < 3; ++FD)
        if (Mask & (1 << FD))
          close(FD);
      Child Proc;
      Proc.start({"--stdio"});
      if (write(Proc.In, "x", 1) != 1)
        return 1;
      Proc.closeInput();
      char Byte;
      if (readReady(Proc.Out, Byte) != 1 || Byte != 'x' ||
          readReady(Proc.Err, Byte) != 1 || Byte != 'e' ||
          readReady(Proc.Out, Byte) != 0)
        return 2;
      return Proc.finish();
    }
  }
  testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
