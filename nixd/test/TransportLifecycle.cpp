#include "../lib/Support/ForkPipedInternal.h"
#include "lspserver/LSPServer.h"
#include "nixd/Eval/AttrSetClient.h"
#include "nixd/Eval/Launch.h"
#include "nixd/Support/ForkPiped.h"

#include <gtest/gtest.h>
#include <llvm/Support/Program.h>

#include <atomic>
#include <barrier>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace nixd {
namespace {

class ExactChildCleanup {
  pid_t PID;

public:
  explicit ExactChildCleanup(pid_t PID) : PID(PID) {}
  void release() { PID = -1; }

  ~ExactChildCleanup() {
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

class TemporaryOutputFile {
  std::string Path;

public:
  TemporaryOutputFile() {
    Path = (std::filesystem::temp_directory_path() /
            "codex-nixd-exec-stderr.XXXXXX")
               .string();
    const int FD = ::mkstemp(Path.data());
    EXPECT_GE(FD, 0);
    if (FD >= 0) {
      (void)::close(FD);
    } else {
      Path.clear();
    }
  }

  ~TemporaryOutputFile() {
    if (!Path.empty())
      (void)::unlink(Path.c_str());
  }

  [[nodiscard]] const std::string &path() const { return Path; }
};

class TemporaryDirectory {
  std::filesystem::path Path;

public:
  TemporaryDirectory() {
    std::string Pattern =
        (std::filesystem::temp_directory_path() / "codex-nixd-exec.XXXXXX")
            .string();
    const char *Created = ::mkdtemp(Pattern.data());
    if (!Created)
      throw std::system_error(errno, std::generic_category(), "mkdtemp");
    Path = Created;
  }

  ~TemporaryDirectory() {
    std::error_code EC;
    std::filesystem::remove_all(Path, EC);
  }

  [[nodiscard]] const std::filesystem::path &path() const { return Path; }
};

class ScopedEnvironment {
  std::string Name;
  std::optional<std::string> Previous;

public:
  ScopedEnvironment(std::string Name, const std::string &Value)
      : Name(std::move(Name)) {
    if (const char *Current = ::getenv(this->Name.c_str()))
      Previous = Current;
    if (::setenv(this->Name.c_str(), Value.c_str(), 1) != 0)
      throw std::system_error(errno, std::generic_category(), "setenv");
  }

  ~ScopedEnvironment() {
    if (Previous)
      (void)::setenv(Name.c_str(), Previous->c_str(), 1);
    else
      (void)::unsetenv(Name.c_str());
  }
};

std::filesystem::path findTestExecutable(llvm::StringRef Name) {
  auto Executable = llvm::sys::findProgramByName(Name);
  EXPECT_TRUE(static_cast<bool>(Executable))
      << "test executable is unavailable: " << Name.str();
  return Executable ? std::filesystem::path(*Executable)
                    : std::filesystem::path();
}

class ExactChildWatchdog {
  pid_t PID;
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Completed = false;
  std::atomic<bool> Fired = false;
  std::thread Thread;

public:
  explicit ExactChildWatchdog(pid_t PID)
      : PID(PID), Thread([this] {
          std::unique_lock Lock(Mutex);
          if (Changed.wait_for(Lock, std::chrono::seconds(2),
                               [this] { return Completed; }))
            return;
          Fired = true;
          siginfo_t Info{};
          errno = 0;
          if (::waitid(P_PID, this->PID, &Info, WEXITED | WNOHANG | WNOWAIT) <
                  0 &&
              errno == ECHILD)
            return;
          (void)::kill(-this->PID, SIGKILL);
          (void)::kill(this->PID, SIGKILL);
        }) {}

  void complete() {
    {
      std::lock_guard Guard(Mutex);
      Completed = true;
    }
    Changed.notify_all();
  }

  ~ExactChildWatchdog() {
    complete();
    Thread.join();
  }

  [[nodiscard]] bool fired() const { return Fired; }
};

class ExactDetachedProcessCleanup {
  pid_t PID = -1;

public:
  void setPID(pid_t NewPID) { PID = NewPID; }
  void release() { PID = -1; }

  ~ExactDetachedProcessCleanup() {
    if (PID <= 0)
      return;
    (void)::kill(-PID, SIGKILL);
    (void)::kill(PID, SIGKILL);
    const auto Deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < Deadline) {
      errno = 0;
      if (::kill(PID, 0) < 0 && errno == ESRCH)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
};

bool writeExact(int FD, const void *Data, size_t Size) {
  const auto *Bytes = static_cast<const char *>(Data);
  size_t Offset = 0;
  while (Offset < Size) {
    const ssize_t Written = ::write(FD, Bytes + Offset, Size - Offset);
    if (Written > 0) {
      Offset += static_cast<size_t>(Written);
      continue;
    }
    if (Written < 0 && errno == EINTR)
      continue;
    return false;
  }
  return true;
}

bool readExact(int FD, void *Data, size_t Size) {
  auto *Bytes = static_cast<char *>(Data);
  size_t Offset = 0;
  while (Offset < Size) {
    const ssize_t Read = ::read(FD, Bytes + Offset, Size - Offset);
    if (Read > 0) {
      Offset += static_cast<size_t>(Read);
      continue;
    }
    if (Read < 0 && errno == EINTR)
      continue;
    return false;
  }
  return true;
}

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

enum class InboundReplyBehavior {
  Delayed,
  Double,
  Never,
  Abandon,
  ExplicitShutdown
};

class InboundReplyServer final : public lspserver::LSPServer {
  InboundReplyBehavior Behavior;

public:
  std::mutex Mutex;
  std::condition_variable Captured;
  std::shared_ptr<lspserver::Callback<llvm::json::Value>> SavedReply;
  std::vector<std::shared_ptr<lspserver::Callback<llvm::json::Value>>>
      SavedReplies;

  InboundReplyServer(std::unique_ptr<lspserver::InboundPort> In,
                     std::unique_ptr<lspserver::OutboundPort> Out,
                     InboundReplyBehavior Behavior)
      : LSPServer(std::move(In), std::move(Out)), Behavior(Behavior) {
    Registry.MethodHandlers["test/inbound"] =
        [this](llvm::json::Value,
               lspserver::Callback<llvm::json::Value> Reply) mutable {
          if (this->Behavior == InboundReplyBehavior::Double) {
            Reply(1);
            Reply(2);
            return;
          }
          if (this->Behavior == InboundReplyBehavior::Abandon)
            return;
          if (this->Behavior == InboundReplyBehavior::ExplicitShutdown)
            closeRequestGate("explicit test shutdown");
          {
            std::lock_guard Guard(Mutex);
            SavedReply =
                std::make_shared<lspserver::Callback<llvm::json::Value>>(
                    std::move(Reply));
            SavedReplies.push_back(SavedReply);
          }
          Captured.notify_one();
        };
  }

  void beginShutdown() { closeRequestGate("explicit test shutdown"); }
};

class BlockingOutputStream final : public llvm::raw_ostream {
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Entered = false;
  bool Released = false;
  std::string Buffer;

  void write_impl(const char *Ptr, size_t Size) override {
    std::unique_lock Lock(Mutex);
    Entered = true;
    Changed.notify_all();
    Changed.wait(Lock, [&] { return Released; });
    Buffer.append(Ptr, Size);
  }

  [[nodiscard]] uint64_t current_pos() const override { return Buffer.size(); }

public:
  bool waitUntilEntered() {
    std::unique_lock Lock(Mutex);
    return Changed.wait_for(Lock, std::chrono::seconds(1),
                            [&] { return Entered; });
  }

  void release() {
    {
      std::lock_guard Guard(Mutex);
      Released = true;
    }
    Changed.notify_all();
  }

  [[nodiscard]] std::string str() {
    std::lock_guard Guard(Mutex);
    return Buffer;
  }
};

void writeAll(int FD, std::string_view Data) {
  while (!Data.empty()) {
    const ssize_t Written = ::write(FD, Data.data(), Data.size());
    ASSERT_GT(Written, 0);
    if (Written <= 0)
      return;
    Data.remove_prefix(static_cast<size_t>(Written));
  }
}

void writeStandardMessage(int FD, llvm::json::Value Message) {
  std::string Body;
  llvm::raw_string_ostream BodyStream(Body);
  BodyStream << Message;
  BodyStream.flush();
  writeAll(FD, "Content-Length: " + std::to_string(Body.size()) + "\r\n\r\n" +
                   Body);
}

void writeInboundCall(int FD, int ID) {
  writeStandardMessage(FD, llvm::json::Object{{"jsonrpc", "2.0"},
                                              {"id", ID},
                                              {"method", "test/inbound"},
                                              {"params", nullptr}});
}

void writeInputExit(int FD) {
  writeStandardMessage(FD, llvm::json::Object{{"jsonrpc", "2.0"},
                                              {"method", "exit"},
                                              {"params", nullptr}});
}

void writeInboundCallAndTerminate(int FD, bool Exit) {
  writeInboundCall(FD, 1);
  if (Exit) {
    writeInputExit(FD);
  }
}

size_t countOccurrences(std::string_view Text, std::string_view Pattern) {
  size_t Count = 0;
  for (size_t Pos = 0;
       (Pos = Text.find(Pattern, Pos)) != std::string_view::npos;
       Pos += Pattern.size())
    ++Count;
  return Count;
}

TEST(TransportLifecycle, BareExitDrainsAlreadyDispatchedInboundReply) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCallAndTerminate(Input[1], true);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Delayed);
  std::atomic<bool> RunReturned = false;
  std::thread Replier([&] {
    std::shared_ptr<lspserver::Callback<llvm::json::Value>> Reply;
    {
      std::unique_lock Lock(Server.Mutex);
      ASSERT_TRUE(Server.Captured.wait_for(Lock, std::chrono::seconds(1), [&] {
        return static_cast<bool>(Server.SavedReply);
      }));
      Reply = Server.SavedReply;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(RunReturned);
    (*Reply)(42);
  });

  Server.run();
  RunReturned = true;
  Replier.join();
  Stream.flush();

  EXPECT_EQ(countOccurrences(Output, "\"result\":42"), 1U);
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, InboundReplyCompletionIsOneShotAndReentrantSafe) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCallAndTerminate(Input[1], true);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Double);
  Server.run();
  Stream.flush();

  EXPECT_EQ(countOccurrences(Output, "\"result\":"), 1U);
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle,
     ConcurrentDuplicateRepliesKeepImmutableIDAndFinishExactlyOnce) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeStandardMessage(Input[1], llvm::json::Object{{"jsonrpc", "2.0"},
                                                    {"id", "duplicate-id"},
                                                    {"method", "test/inbound"},
                                                    {"params", nullptr}});
  writeInputExit(Input[1]);

  BlockingOutputStream Output;
  std::string Logs;
  llvm::raw_string_ostream LogStream(Logs);
  lspserver::StreamLogger Logger(LogStream, lspserver::Logger::Debug);
  lspserver::LoggingSession Logging(Logger);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Output),
                            InboundReplyBehavior::Delayed);

  std::atomic<unsigned> ReturnedReplies = 0;
  std::thread Replier([&] {
    std::shared_ptr<lspserver::Callback<llvm::json::Value>> Reply;
    {
      std::unique_lock Lock(Server.Mutex);
      ASSERT_TRUE(Server.Captured.wait_for(Lock, std::chrono::seconds(1), [&] {
        return static_cast<bool>(Server.SavedReply);
      }));
      Reply = Server.SavedReply;
    }
    std::barrier Start(3);
    std::thread First([&] {
      Start.arrive_and_wait();
      (*Reply)(1);
      ++ReturnedReplies;
    });
    std::thread Second([&] {
      Start.arrive_and_wait();
      (*Reply)(2);
      ++ReturnedReplies;
    });
    Start.arrive_and_wait();
    ASSERT_TRUE(Output.waitUntilEntered());
    const auto Deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (ReturnedReplies.load() == 0 &&
           std::chrono::steady_clock::now() < Deadline)
      std::this_thread::yield();
    EXPECT_EQ(ReturnedReplies.load(), 1U);
    Output.release();
    First.join();
    Second.join();
  });

  Server.run();
  Replier.join();
  Output.flush();
  LogStream.flush();

  EXPECT_EQ(ReturnedReplies.load(), 2U);
  EXPECT_EQ(countOccurrences(Output.str(), "\"result\":"), 1U);
  EXPECT_NE(
      Logs.find("ignored duplicate reply for test/inbound(\"duplicate-id\")"),
      std::string::npos)
      << Logs;
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, InboundDrainTimeoutBoundsNeverReplyHandler) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCallAndTerminate(Input[1], true);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Never);
  const auto Start = std::chrono::steady_clock::now();
  Server.run();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;

  EXPECT_GE(Elapsed, std::chrono::milliseconds(400));
  EXPECT_LT(Elapsed, std::chrono::seconds(2));
  ASSERT_TRUE(Server.SavedReply);
  (*Server.SavedReply)(42);
  Stream.flush();
  EXPECT_EQ(countOccurrences(Output, "\"result\":42"), 1U);
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, InboundDrainTracksMultipleDispatchedCalls) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCall(Input[1], 1);
  writeInboundCall(Input[1], 2);
  writeInputExit(Input[1]);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Delayed);
  std::thread Replier([&] {
    std::vector<std::shared_ptr<lspserver::Callback<llvm::json::Value>>>
        Replies;
    {
      std::unique_lock Lock(Server.Mutex);
      ASSERT_TRUE(Server.Captured.wait_for(Lock, std::chrono::seconds(1), [&] {
        return Server.SavedReplies.size() == 2;
      }));
      Replies = Server.SavedReplies;
    }
    (*Replies[0])(41);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    (*Replies[1])(42);
  });

  Server.run();
  Replier.join();
  Stream.flush();

  EXPECT_EQ(countOccurrences(Output, "\"result\":"), 2U);
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, AbandonedInboundReplyReleasesDrainAccounting) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCallAndTerminate(Input[1], true);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Abandon);
  const auto Start = std::chrono::steady_clock::now();
  Server.run();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;

  EXPECT_LT(Elapsed, std::chrono::milliseconds(400));
  EXPECT_TRUE(Output.empty());
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, ExplicitShutdownAndRejectedCallsSkipInputGrace) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCallAndTerminate(Input[1], true);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Never);
  Server.beginShutdown();
  const auto Start = std::chrono::steady_clock::now();
  Server.run();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;
  Stream.flush();

  EXPECT_LT(Elapsed, std::chrono::milliseconds(400));
  EXPECT_FALSE(Server.SavedReply);
  EXPECT_NE(Output.find("server is shutting down"), std::string::npos);
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, CountedExplicitShutdownHandlerDoesNotSelfWait) {
  int Input[2];
  ASSERT_EQ(::pipe(Input), 0);
  writeInboundCallAndTerminate(Input[1], true);

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::ExplicitShutdown);
  const auto Start = std::chrono::steady_clock::now();
  Server.run();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;

  EXPECT_LT(Elapsed, std::chrono::milliseconds(400));
  EXPECT_TRUE(Server.SavedReply);
  EXPECT_EQ(::close(Input[1]), 0);
  EXPECT_EQ(::close(Input[0]), 0);
}

TEST(TransportLifecycle, EOFAndBrokenOutputStillReleaseInboundDrain) {
  const auto PreviousSIGPIPE = ::signal(SIGPIPE, SIG_IGN);
  ASSERT_NE(PreviousSIGPIPE, SIG_ERR);
  int Input[2];
  int Output[2];
  ASSERT_EQ(::pipe(Input), 0);
  ASSERT_EQ(::pipe(Output), 0);
  writeInboundCallAndTerminate(Input[1], false);
  ASSERT_EQ(::close(Output[0]), 0);

  llvm::raw_fd_ostream Stream(Output[1], false);
  InboundReplyServer Server(std::make_unique<lspserver::InboundPort>(Input[0]),
                            std::make_unique<lspserver::OutboundPort>(Stream),
                            InboundReplyBehavior::Delayed);
  std::thread Replier([&] {
    std::shared_ptr<lspserver::Callback<llvm::json::Value>> Reply;
    {
      std::unique_lock Lock(Server.Mutex);
      ASSERT_TRUE(Server.Captured.wait_for(Lock, std::chrono::seconds(1), [&] {
        return static_cast<bool>(Server.SavedReply);
      }));
      Reply = Server.SavedReply;
    }
    EXPECT_EQ(::close(Input[1]), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    (*Reply)(42);
  });

  const auto Start = std::chrono::steady_clock::now();
  Server.run();
  const auto Elapsed = std::chrono::steady_clock::now() - Start;
  Replier.join();

  EXPECT_LT(Elapsed, std::chrono::milliseconds(400));
  EXPECT_EQ(::close(Input[0]), 0);
  Stream.clear_error();
  EXPECT_EQ(::close(Output[1]), 0);
  EXPECT_NE(::signal(SIGPIPE, PreviousSIGPIPE), SIG_ERR);
}

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
    const std::array ChildFDs{Ready[1]};
    AttrSetClientProc Process(
        [&] {
          ::signal(SIGTERM, SIG_IGN);
          const char Byte = 'x';
          if (::write(Ready[1], &Byte, 1) != 1)
            return 2;
          for (;;)
            ::pause();
        },
        {}, ChildFDs);
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

TEST(TransportLifecycle, CooperativeDedicatedGroupUsesSharedGraceBeforeReap) {
  AttrSetClientProc Process([] {
    ::signal(SIGTERM, SIG_DFL);
    for (;;)
      ::pause();
    return 0;
  });
  const pid_t PID = Process.pid();
  ExactChildCleanup Cleanup(PID);
  ExactChildWatchdog Watchdog(PID);

  const auto Start = std::chrono::steady_clock::now();
  EXPECT_TRUE(Process.stop());
  const auto Elapsed = std::chrono::steady_clock::now() - Start;
  Watchdog.complete();

  EXPECT_FALSE(Watchdog.fired());
  EXPECT_GE(Elapsed, std::chrono::milliseconds(450));
  EXPECT_LT(Elapsed, std::chrono::milliseconds(900));
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
  const std::array ChildFDs{Ready[1]};

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
      },
      ChildFDs);
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
  const std::array ChildFDs{Go[0]};

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
      },
      ChildFDs);
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
    const std::array ChildFDs{DescendantPipe[1]};
    AttrSetClientProc Process(
        [&] {
          ::signal(SIGTERM, SIG_IGN);
          const pid_t Child = ::fork();
          if (Child < 0)
            return 2;
          if (Child == 0) {
            ::signal(SIGTERM, SIG_IGN);
            for (;;)
              ::pause();
          }
          if (::write(DescendantPipe[1], &Child, sizeof(Child)) !=
              sizeof(Child))
            return 3;
          for (;;)
            ::pause();
        },
        {}, ChildFDs);
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
    const std::array ChildFDs{DescendantPipe[1]};
    AttrSetClientProc Process(
        [&] {
          const pid_t Child = ::fork();
          if (Child < 0)
            return 2;
          if (Child == 0) {
            for (;;)
              ::pause();
          }
          if (::write(DescendantPipe[1], &Child, sizeof(Child)) !=
              sizeof(Child))
            return 3;
          return 0;
        },
        {}, ChildFDs);
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
    const std::array ChildFDs{DescendantPipe[1]};
    AttrSetClientProc Process(
        [&] {
          const pid_t Child = ::fork();
          if (Child < 0)
            return 2;
          if (Child == 0) {
            ::signal(SIGTERM, SIG_IGN);
            for (;;)
              ::pause();
          }
          if (::write(DescendantPipe[1], &Child, sizeof(Child)) !=
              sizeof(Child))
            return 3;
          return 0;
        },
        {}, ChildFDs);
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

TEST(TransportLifecycle, ForkPipedRoutesAllStreamsWhenParentStdioIsClosed) {
  int Report[2];
  ASSERT_EQ(::pipe(Report), 0);
  const pid_t Outer = ::fork();
  if (Outer < 0) {
    (void)::close(Report[0]);
    (void)::close(Report[1]);
    FAIL() << "failed to launch isolated closed-stdio test process";
  }
  if (Outer == 0) {
    (void)::close(Report[0]);
    (void)::close(STDIN_FILENO);
    (void)::close(STDOUT_FILENO);
    (void)::close(STDERR_FILENO);
    (void)::signal(SIGPIPE, SIG_IGN);
    const std::array ChildFDs{Report[1]};
    int In = -1;
    int Out = -1;
    int Err = -1;
    const pid_t Child = forkPiped(In, Out, Err, nullptr, ChildFDs);
    if (Child == 0) {
      (void)::alarm(2);
      char Input = '?';
      const ssize_t Read = ::read(STDIN_FILENO, &Input, 1);
      static constexpr char Stdout[] = "stdout:";
      static constexpr char Stderr[] = "stderr";
      const bool Wrote =
          writeExact(STDOUT_FILENO, Stdout, sizeof(Stdout) - 1) &&
          writeExact(STDOUT_FILENO, &Input, 1) &&
          writeExact(STDERR_FILENO, Stderr, sizeof(Stderr) - 1);
      _exit(Read == 1 && Wrote ? 0 : 3);
    }
    if (Child < 0 || !writeExact(Report[1], &Child, sizeof(Child)))
      _exit(4);

    const char Input = 'I';
    const bool WroteInput = writeExact(In, &Input, 1);
    (void)::close(In);
    const std::string Stdout = readAll(Out);
    const std::string Stderr = readAll(Err);
    (void)::close(Out);
    (void)::close(Err);
    int Status = 0;
    const bool Reaped = ::waitpid(Child, &Status, 0) == Child;
    const uint8_t Success =
        WroteInput && Stdout == "stdout:I" && Stderr == "stderr" && Reaped &&
                WIFEXITED(Status) && WEXITSTATUS(Status) == 0
            ? 1
            : 0;
    (void)writeExact(Report[1], &Success, sizeof(Success));
    (void)::close(Report[1]);
    _exit(0);
  }

  ExactChildCleanup OuterCleanup(Outer);
  ExactDetachedProcessCleanup ChildCleanup;
  (void)::close(Report[1]);
  pid_t Child = -1;
  const bool ReceivedPID = readExact(Report[0], &Child, sizeof(Child));
  if (ReceivedPID && Child > 0)
    ChildCleanup.setPID(Child);
  uint8_t Success = 0;
  const bool ReceivedResult = readExact(Report[0], &Success, sizeof(Success));
  (void)::close(Report[0]);
  if (ReceivedResult && Success == 1)
    ChildCleanup.release();
  int OuterStatus = 0;
  const pid_t ReapedOuter = ::waitpid(Outer, &OuterStatus, 0);

  EXPECT_TRUE(ReceivedPID);
  EXPECT_GT(Child, 0);
  EXPECT_TRUE(ReceivedResult);
  EXPECT_EQ(Success, 1);
  EXPECT_EQ(ReapedOuter, Outer);
  EXPECT_TRUE(WIFEXITED(OuterStatus));
  EXPECT_EQ(WEXITSTATUS(OuterStatus), 0);
}

TEST(TransportLifecycle, StreamProcExecRoutesPipesAndRedirectsStderr) {
  TemporaryOutputFile Stderr;
  ASSERT_FALSE(Stderr.path().empty());
  const auto Shell = findTestExecutable("sh");
  ASSERT_FALSE(Shell.empty());
  util::AutoCloseFD InheritedFD(::open("/dev/null", O_RDONLY));
  ASSERT_GT(InheritedFD.get(), STDERR_FILENO);
  const int InheritedFlags = ::fcntl(InheritedFD.get(), F_GETFD);
  ASSERT_GE(InheritedFlags, 0);
  ASSERT_EQ(::fcntl(InheritedFD.get(), F_SETFD, InheritedFlags & ~FD_CLOEXEC),
            0);
  StreamProc Process(ExecSpec{
      .Executable = Shell,
      .Arguments = {"nixd-test", "-c",
                    "if eval \"true <&$1\" 2>/dev/null; then exit 9; fi; "
                    "read value; printf 'stdout:%s' \"$value\"; "
                    "printf 'stderr:%s' \"$value\" >&2",
                    "nixd-test", std::to_string(InheritedFD.get())},
      .Stderr = Stderr.path(),
  });
  ExactChildCleanup Cleanup(Process.proc().PID);
  const pid_t PID = Process.proc().PID;
  Process.claimProcess();

  ASSERT_GT(PID, 0);
  EXPECT_EQ(Process.proc().ProcessGroup, PID);
  EXPECT_NE(::fcntl(Process.proc().Stdin.get(), F_GETFD) & FD_CLOEXEC, 0);
  EXPECT_NE(::fcntl(Process.proc().Stdout.get(), F_GETFD) & FD_CLOEXEC, 0);
  ASSERT_TRUE(writeExact(Process.proc().Stdin.get(), "input\n", 6));
  ASSERT_EQ(::close(Process.proc().Stdin.get()), 0);
  Process.proc().Stdin.release();

  const std::string Stdout = readAll(Process.proc().Stdout.get());
  int Status = 0;
  ASSERT_EQ(::waitpid(PID, &Status, 0), PID);
  Cleanup.release();
  const int StderrFD = ::open(Stderr.path().c_str(), O_RDONLY);
  ASSERT_GE(StderrFD, 0);
  const std::string Error = readAll(StderrFD);
  ASSERT_EQ(::close(StderrFD), 0);

  ASSERT_TRUE(WIFEXITED(Status));
  EXPECT_EQ(WEXITSTATUS(Status), 0);
  EXPECT_EQ(Stdout, "stdout:input");
  EXPECT_EQ(Error, "stderr:input");
}

TEST(TransportLifecycle,
     StreamProcExecOpensRelativeStderrBeforeEnteringWorkingDirectory) {
  TemporaryDirectory Temp;
  const auto Selected = Temp.path() / "selected";
  ASSERT_TRUE(std::filesystem::create_directory(Selected));
  const auto Stderr = Temp.path() / "stderr";
  std::error_code EC;
  const auto RelativeStderr =
      std::filesystem::relative(Stderr, std::filesystem::current_path(), EC);
  ASSERT_FALSE(EC) << EC.message();
  const auto Shell = findTestExecutable("sh");
  ASSERT_FALSE(Shell.empty());
  StreamProc Process(ExecSpec{
      .Executable = Shell,
      .Arguments = {"nixd-test", "-c",
                    "printf 'relative-stderr' >&2; read ignored || :"},
      .Stderr = RelativeStderr,
      .WorkingDirectory = Selected,
  });
  const pid_t Child = Process.proc().PID;
  ExactChildCleanup Cleanup(Child);
  Process.claimProcess();
  ASSERT_EQ(::close(Process.proc().Stdin.get()), 0);
  Process.proc().Stdin.release();
  (void)readAll(Process.proc().Stdout.get());
  int Status = 0;
  ASSERT_EQ(::waitpid(Child, &Status, 0), Child);
  Cleanup.release();

  ASSERT_TRUE(WIFEXITED(Status));
  EXPECT_EQ(WEXITSTATUS(Status), 0);
  std::ifstream Input(Stderr);
  std::string Error;
  ASSERT_TRUE(static_cast<bool>(std::getline(Input, Error)));
  EXPECT_EQ(Error, "relative-stderr");
}

TEST(TransportLifecycle,
     StartAttrSetEvalRunsUnmodifiedArgvInSelectedWorkingDirectory) {
  TemporaryDirectory Temp;
  const auto Selected = Temp.path() / "selected";
  ASSERT_TRUE(std::filesystem::create_directory(Selected));
  const auto Wrapper = Selected / "relative-evaluator";
  const auto Report = Temp.path() / "report";
  {
    std::ofstream Output(Wrapper);
    ASSERT_TRUE(Output.good());
    Output << "#!/bin/sh\n"
              "if [ \"$#\" -ne 0 ]; then\n"
              "  printf 'args:%s\\n' \"$#\" > \"$NIXD_TEST_EVAL_REPORT\"\n"
              "  exit 97\n"
              "fi\n"
              "pwd > \"$NIXD_TEST_EVAL_REPORT\"\n"
              "while :; do sleep 1; done\n";
  }
  ASSERT_EQ(::chmod(Wrapper.c_str(), 0700), 0);
  ScopedEnvironment Evaluator("NIXD_ATTRSET_EVAL", "relative-evaluator");
  ScopedEnvironment ReportPath("NIXD_TEST_EVAL_REPORT", Report.string());
  TemporaryOutputFile Stderr;
  ASSERT_FALSE(Stderr.path().empty());
  std::error_code RelativeEC;
  const auto RelativeSelected = std::filesystem::relative(
      Selected, std::filesystem::current_path(), RelativeEC);
  ASSERT_FALSE(RelativeEC) << RelativeEC.message();
  ASSERT_FALSE(RelativeSelected.is_absolute());

  std::unique_ptr<AttrSetClientProc> Worker;
  startAttrSetEval(Stderr.path(), Worker, RelativeSelected);
  ASSERT_TRUE(Worker);

  const auto Deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  std::string Actual;
  while (std::chrono::steady_clock::now() < Deadline) {
    std::ifstream Input(Report);
    if (std::getline(Input, Actual))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_FALSE(Actual.empty());
  std::error_code EquivalentEC;
  EXPECT_TRUE(std::filesystem::equivalent(Actual, Selected, EquivalentEC));
  EXPECT_FALSE(EquivalentEC) << EquivalentEC.message();
}

TEST(TransportLifecycle, UnclaimedStreamProcDestructionKillsAndReapsChild) {
  TemporaryOutputFile Stderr;
  ASSERT_FALSE(Stderr.path().empty());
  const auto Shell = findTestExecutable("sh");
  ASSERT_FALSE(Shell.empty());
  pid_t Child = -1;
  std::unique_ptr<ExactChildCleanup> Cleanup;
  {
    StreamProc Process(ExecSpec{
        .Executable = Shell,
        .Arguments = {"nixd-test", "-c",
                      "trap '' TERM HUP; while :; do sleep 1; done"},
        .Stderr = Stderr.path(),
    });
    Child = Process.proc().PID;
    ASSERT_GT(Child, 0);
    Cleanup = std::make_unique<ExactChildCleanup>(Child);
  }

  int Status = 0;
  errno = 0;
  EXPECT_EQ(::waitpid(Child, &Status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
  if (errno == ECHILD)
    Cleanup->release();
}

TEST(TransportLifecycle, StreamProcExecOpenFailureDoesNotLeakDescriptors) {
  const auto Shell = findTestExecutable("sh");
  ASSERT_FALSE(Shell.empty());
  TemporaryOutputFile BlockingParent;
  ASSERT_FALSE(BlockingParent.path().empty());
  const auto InvalidStderr =
      std::filesystem::path(BlockingParent.path()) / "child";
  const size_t Before = countOpenDescriptors();

  try {
    StreamProc Process(ExecSpec{
        .Executable = Shell,
        .Arguments = {"nixd-test"},
        .Stderr = InvalidStderr,
    });
    FAIL() << "launch unexpectedly accepted a regular file as a directory";
  } catch (const std::system_error &Error) {
    EXPECT_EQ(Error.code(), std::error_code(ENOTDIR, std::generic_category()));
  }

  EXPECT_EQ(countOpenDescriptors(), Before);
}

TEST(TransportLifecycle, StreamProcExecRoutesPipesWhenParentStdioIsClosed) {
  TemporaryOutputFile Stderr;
  ASSERT_FALSE(Stderr.path().empty());
  const auto Shell = findTestExecutable("sh");
  ASSERT_FALSE(Shell.empty());
  int Report[2];
  ASSERT_EQ(::pipe(Report), 0);
  const pid_t Outer = ::fork();
  if (Outer < 0) {
    (void)::close(Report[0]);
    (void)::close(Report[1]);
    FAIL() << "failed to launch isolated closed-stdio exec test process";
  }
  if (Outer == 0) {
    (void)::close(Report[0]);
    (void)::close(STDIN_FILENO);
    (void)::close(STDOUT_FILENO);
    (void)::close(STDERR_FILENO);
    (void)::signal(SIGPIPE, SIG_IGN);
    (void)::alarm(3);
    try {
      StreamProc Process(ExecSpec{
          .Executable = Shell,
          .Arguments = {"nixd-test", "-c",
                        "read value; printf 'stdout:%s' \"$value\""},
          .Stderr = Stderr.path(),
      });
      const pid_t Child = Process.proc().PID;
      if (!writeExact(Report[1], &Child, sizeof(Child)))
        throw std::system_error(EPIPE, std::generic_category());
      Process.claimProcess();
      const bool Wrote = writeExact(Process.proc().Stdin.get(), "input\n", 6);
      (void)::close(Process.proc().Stdin.get());
      Process.proc().Stdin.release();
      const std::string Output = readAll(Process.proc().Stdout.get());
      int Status = 0;
      const bool Reaped = ::waitpid(Child, &Status, 0) == Child;
      const uint8_t Success = Wrote && Output == "stdout:input" && Reaped &&
                                      WIFEXITED(Status) &&
                                      WEXITSTATUS(Status) == 0
                                  ? 1
                                  : 0;
      (void)writeExact(Report[1], &Success, sizeof(Success));
      (void)::close(Report[1]);
      _exit(0);
    } catch (...) {
      _exit(3);
    }
  }

  ExactChildCleanup OuterCleanup(Outer);
  ExactDetachedProcessCleanup ChildCleanup;
  (void)::close(Report[1]);
  pid_t Child = -1;
  const bool ReceivedPID = readExact(Report[0], &Child, sizeof(Child));
  if (ReceivedPID && Child > 0)
    ChildCleanup.setPID(Child);
  uint8_t Success = 0;
  const bool ReceivedResult = readExact(Report[0], &Success, sizeof(Success));
  (void)::close(Report[0]);
  if (ReceivedResult && Success == 1)
    ChildCleanup.release();
  int Status = 0;
  const pid_t Reaped = ::waitpid(Outer, &Status, 0);
  if (Reaped == Outer)
    OuterCleanup.release();

  EXPECT_TRUE(ReceivedPID);
  EXPECT_GT(Child, 0);
  EXPECT_TRUE(ReceivedResult);
  EXPECT_EQ(Success, 1);
  EXPECT_EQ(Reaped, Outer);
  ASSERT_TRUE(WIFEXITED(Status));
  EXPECT_EQ(WEXITSTATUS(Status), 0);
}

TEST(TransportLifecycle, ForkPipedRetriesDup2AfterEINTR) {
  bool Interrupted = false;
  detail::ForkPipedSyscalls Syscalls{
      .Pipe = [](int *FDs) { return ::pipe(FDs); },
      .Fork = [] { return ::fork(); },
      .Dup2 =
          [&](int OldFD, int NewFD) {
            if (NewFD == STDOUT_FILENO && !Interrupted) {
              Interrupted = true;
              errno = EINTR;
              return -1;
            }
            return ::dup2(OldFD, NewFD);
          },
  };
  int In = -1;
  int Out = -1;
  int Err = -1;
  const pid_t Child = detail::forkPipedWith(In, Out, Err, nullptr, Syscalls);
  if (Child == 0) {
    static constexpr char Message[] = "retry";
    _exit(writeExact(STDOUT_FILENO, Message, sizeof(Message) - 1) ? 0 : 3);
  }

  ExactChildCleanup Cleanup(Child);
  (void)::close(In);
  const std::string Stdout = readAll(Out);
  const std::string Stderr = readAll(Err);
  (void)::close(Out);
  (void)::close(Err);
  int Status = 0;
  const pid_t Reaped = ::waitpid(Child, &Status, 0);

  EXPECT_EQ(Stdout, "retry");
  EXPECT_TRUE(Stderr.empty());
  EXPECT_EQ(Reaped, Child);
  ASSERT_TRUE(WIFEXITED(Status));
  EXPECT_EQ(WEXITSTATUS(Status), 0);
}

TEST(TransportLifecycle, ForkPipedExitsChildWhenDup2Fails) {
  detail::ForkPipedSyscalls Syscalls{
      .Pipe = [](int *FDs) { return ::pipe(FDs); },
      .Fork = [] { return ::fork(); },
      .Dup2 =
          [](int OldFD, int NewFD) {
            if (NewFD == STDERR_FILENO) {
              errno = EBADF;
              return -1;
            }
            return ::dup2(OldFD, NewFD);
          },
  };
  int In = -1;
  int Out = -1;
  int Err = -1;
  const pid_t Child = detail::forkPipedWith(In, Out, Err, nullptr, Syscalls);
  if (Child == 0)
    _exit(0);

  ExactChildCleanup Cleanup(Child);
  (void)::close(In);
  (void)::close(Out);
  (void)::close(Err);
  int Status = 0;
  const pid_t Reaped = ::waitpid(Child, &Status, 0);

  EXPECT_EQ(Reaped, Child);
  ASSERT_TRUE(WIFEXITED(Status));
  EXPECT_EQ(WEXITSTATUS(Status), 126);
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
