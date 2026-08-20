#include "nixd/Controller/Formatter.h"
#include "FormatterInternal.h"
#include "nixd/Support/ForkPiped.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

using namespace nixd;

void nixd::detail::closeOwnedWith(
    util::AutoCloseFD &FD, const std::function<int(int)> &Close) noexcept {
  if (FD.isReleased())
    return;
  const int RawFD = FD.get();
  // POSIX permits close to release the descriptor even when reporting EINTR.
  // Drop ownership before the one syscall so neither this path nor the RAII
  // destructor can close a subsequently reused descriptor number.
  FD.release();
  try {
    (void)Close(RawFD);
  } catch (...) {
    // Descriptor cleanup is no-throw and never retries an indeterminate close.
  }
}

namespace {

void closeOwned(util::AutoCloseFD &FD) noexcept {
  detail::closeOwnedWith(FD, [](int RawFD) { return ::close(RawFD); });
}

void makeNonBlocking(const util::AutoCloseFD &FD) {
  const int Flags = ::fcntl(FD.get(), F_GETFL);
  if (Flags < 0 || ::fcntl(FD.get(), F_SETFL, Flags | O_NONBLOCK) < 0)
    throw std::system_error(errno, std::generic_category());
}

void suppressSIGPIPE(const util::AutoCloseFD &FD) {
#ifdef F_SETNOSIGPIPE
  if (::fcntl(FD.get(), F_SETNOSIGPIPE, 1) < 0)
    throw std::system_error(errno, std::generic_category());
#else
  (void)FD;
#endif
}

class ScopedSIGPIPEBlock {
  sigset_t Set{};
  sigset_t OldSet{};
  bool Active = false;
  bool WasPending = false;

public:
  ScopedSIGPIPEBlock() {
    sigemptyset(&Set);
    sigaddset(&Set, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &Set, &OldSet) != 0)
      return;
    Active = true;
    sigset_t Pending{};
    if (sigpending(&Pending) == 0)
      WasPending = sigismember(&Pending, SIGPIPE) == 1;
  }

  void consumeGeneratedSignal() {
    if (!Active || WasPending)
      return;
    sigset_t Pending{};
    if (sigpending(&Pending) != 0 || sigismember(&Pending, SIGPIPE) != 1)
      return;
    int Signal = 0;
    while (sigwait(&Set, &Signal) == EINTR) {
    }
  }

  ~ScopedSIGPIPEBlock() {
    if (Active) {
      consumeGeneratedSignal();
      pthread_sigmask(SIG_SETMASK, &OldSet, nullptr);
    }
  }
};

class FormatterOwner {
  FormatterProcessRegistry &Registry;
  FormatterProcess Process;

public:
  FormatterOwner(FormatterProcessRegistry &Registry, FormatterProcess Process)
      : Registry(Registry), Process(std::move(Process)) {}

  ~FormatterOwner() {
    closeDescriptors();
    if (Process.Identity->ownsIdentity()) {
      Registry.cancel(Process.Identity);
      int Status = 0;
      (void)Registry.reap(Process.Identity, Status, 0);
    }
    Registry.deregister(Process.Identity);
  }

  FormatterProcess &process() { return Process; }

  void closeDescriptors() noexcept {
    closeOwned(Process.Stdin);
    closeOwned(Process.Stdout);
    closeOwned(Process.Stderr);
  }

  int cancelAndReap() noexcept {
    closeDescriptors();
    Registry.cancel(Process.Identity);
    int Status = 0;
    const pid_t Reaped = Registry.reap(Process.Identity, Status, 0);
    return Reaped == Process.Identity->pid() ? Status : -1;
  }
};

void drain(util::AutoCloseFD &FD, std::string &Output) {
  if (FD.isReleased())
    return;
  std::array<char, 4096> Buffer{};
  for (;;) {
    const ssize_t Read = ::read(FD.get(), Buffer.data(), Buffer.size());
    if (Read > 0) {
      Output.append(Buffer.data(), static_cast<size_t>(Read));
      continue;
    }
    if (Read == 0) {
      closeOwned(FD);
      return;
    }
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return;
    throw std::system_error(errno, std::generic_category());
  }
}

} // namespace

FormatterProcess::FormatterProcess(
    std::shared_ptr<ProcessTreeIdentity> Identity, util::PipedProc Process)
    : Identity(std::move(Identity)), Stdin(std::move(Process.Stdin)),
      Stdout(std::move(Process.Stdout)), Stderr(std::move(Process.Stderr)) {}

FormatterProcessRegistry::FormatterProcessRegistry(ProcessTreeBackend Backend)
    : Backend(std::move(Backend)) {}

std::optional<FormatterProcess>
FormatterProcessRegistry::launch(const Launcher &Launcher) {
  std::lock_guard Guard(Mutex);
  ++LaunchAttempts;
  if (!Accepting)
    return std::nullopt;

  auto Identity = std::make_shared<ProcessTreeIdentity>();
  Active.push_back(Identity);
  try {
    util::PipedProc Process = Launcher();
    Identity->setProcess(Process.PID, Process.ProcessGroup);
    return FormatterProcess(std::move(Identity), std::move(Process));
  } catch (...) {
    std::erase(Active, Identity);
    throw;
  }
}

void FormatterProcessRegistry::deregister(
    const std::shared_ptr<ProcessTreeIdentity> &Identity) {
  std::lock_guard Guard(Mutex);
  std::erase(Active, Identity);
}

void FormatterProcessRegistry::cancel(
    const std::shared_ptr<ProcessTreeIdentity> &Identity) noexcept {
  const std::array Trees{Identity};
  cancelProcessTrees(Trees, Backend);
}

bool FormatterProcessRegistry::terminateCompletedOwnedGroup(
    const std::shared_ptr<ProcessTreeIdentity> &Identity) noexcept {
  return Identity && Identity->terminateCompletedOwnedGroup(Backend);
}

pid_t FormatterProcessRegistry::reap(
    const std::shared_ptr<ProcessTreeIdentity> &Identity, int &Status,
    int Options) noexcept {
  if (!Identity) {
    errno = ECHILD;
    return -1;
  }
  return Identity->reapChild(Status, Options, Backend);
}

void FormatterProcessRegistry::cancelAll() noexcept {
  std::vector<std::shared_ptr<ProcessTreeIdentity>> Snapshot;
  {
    std::lock_guard Guard(Mutex);
    Accepting = false;
    Snapshot = Active;
  }
  cancelProcessTrees(Snapshot, Backend);
}

std::vector<pid_t> FormatterProcessRegistry::activePIDs() const {
  std::lock_guard Guard(Mutex);
  std::vector<pid_t> Result;
  Result.reserve(Active.size());
  for (const auto &Identity : Active) {
    if (const pid_t PID = Identity->pid(); PID > 0)
      Result.push_back(PID);
  }
  return Result;
}

bool FormatterProcessRegistry::allActiveCancellationRequested() const {
  std::lock_guard Guard(Mutex);
  return std::ranges::all_of(Active, [](const auto &Identity) {
    return Identity->cancellationRequested();
  });
}

size_t FormatterProcessRegistry::launchAttempts() const {
  std::lock_guard Guard(Mutex);
  return LaunchAttempts;
}

FormatterRunResult nixd::runFormatter(FormatterProcessRegistry &Registry,
                                      const std::vector<std::string> &Command,
                                      const std::filesystem::path &CWD,
                                      std::string_view Input) {
  if (Command.empty())
    throw std::invalid_argument("formatter command is empty");

  std::vector<char *> Args;
  Args.reserve(Command.size() + 1);
  for (const std::string &Arg : Command)
    Args.push_back(const_cast<char *>(Arg.c_str()));
  Args.push_back(nullptr);

  auto Launched = Registry.launch([&] {
    int In = -1;
    int Out = -1;
    int Err = -1;
    pid_t ProcessGroup = -1;
    const pid_t Child = forkPiped(In, Out, Err, &ProcessGroup);
    if (Child == 0) {
      if (::chdir(CWD.c_str()) != 0)
        _exit(127);
      ::execvp(Args.front(), Args.data());
      _exit(127);
    }
    return util::PipedProc(Child, ProcessGroup, In, Out, Err);
  });

  FormatterRunResult Result;
  if (!Launched) {
    Result.Cancelled = true;
    return Result;
  }

  FormatterOwner Owner(Registry, std::move(*Launched));
  auto &Process = Owner.process();
  ScopedSIGPIPEBlock BlockSIGPIPE;
  suppressSIGPIPE(Process.Stdin);
  makeNonBlocking(Process.Stdin);
  makeNonBlocking(Process.Stdout);
  makeNonBlocking(Process.Stderr);

  size_t InputOffset = 0;
  if (Input.empty())
    closeOwned(Process.Stdin);

  while (!Process.Stdin.isReleased() || !Process.Stdout.isReleased() ||
         !Process.Stderr.isReleased()) {
    if (Process.Identity->cancellationRequested()) {
      Result.Cancelled = true;
      Result.ExitStatus = Owner.cancelAndReap();
      return Result;
    }

    std::array<pollfd, 3> FDs{{
        {Process.Stdin.isReleased() ? -1 : Process.Stdin.get(),
         static_cast<short>(Process.Stdin.isReleased() ? 0 : POLLOUT), 0},
        {Process.Stdout.isReleased() ? -1 : Process.Stdout.get(),
         static_cast<short>(Process.Stdout.isReleased() ? 0 : POLLIN), 0},
        {Process.Stderr.isReleased() ? -1 : Process.Stderr.get(),
         static_cast<short>(Process.Stderr.isReleased() ? 0 : POLLIN), 0},
    }};
    int Ready;
    do {
      Ready = ::poll(FDs.data(), FDs.size(), 20);
    } while (Ready < 0 && errno == EINTR);
    if (Ready < 0)
      throw std::system_error(errno, std::generic_category());
    if (Ready == 0)
      continue;

    if (FDs[1].revents & (POLLIN | POLLHUP | POLLERR))
      drain(Process.Stdout, Result.Stdout);
    if (FDs[2].revents & (POLLIN | POLLHUP | POLLERR))
      drain(Process.Stderr, Result.Stderr);
    if (FDs[0].revents & POLLNVAL)
      throw std::system_error(EBADF, std::generic_category());
    if (FDs[0].revents & (POLLOUT | POLLHUP | POLLERR)) {
      const ssize_t Written =
          ::write(Process.Stdin.get(), Input.data() + InputOffset,
                  Input.size() - InputOffset);
      if (Written > 0) {
        InputOffset += static_cast<size_t>(Written);
        if (InputOffset == Input.size())
          closeOwned(Process.Stdin);
      } else if (Written < 0 && errno == EPIPE) {
        const int Failure = errno;
        BlockSIGPIPE.consumeGeneratedSignal();
        if (Process.Identity->cancellationRequested()) {
          Result.Cancelled = true;
          Result.ExitStatus = Owner.cancelAndReap();
          return Result;
        }
        throw std::system_error(Failure, std::generic_category());
      } else if (Written < 0 && errno != EINTR && errno != EAGAIN &&
                 errno != EWOULDBLOCK) {
        throw std::system_error(errno, std::generic_category());
      }
    }
  }

  for (;;) {
    if (Process.Identity->cancellationRequested()) {
      Result.Cancelled = true;
      Result.ExitStatus = Owner.cancelAndReap();
      return Result;
    }
    if (Process.Identity->observeLeaderExit()) {
      // Completion that wins arbitration force-cleans background members while
      // the unreaped leader pins PGID. A winning shutdown keeps that barrier,
      // receives its full grace, and releases this path only when finished.
      const bool CompletionWon =
          Registry.terminateCompletedOwnedGroup(Process.Identity);
      int Status = 0;
      const pid_t Reaped = Registry.reap(Process.Identity, Status, 0);
      if (Reaped != Process.Identity->pid() && !(Reaped < 0 && errno == ECHILD))
        throw std::system_error(errno, std::generic_category());
      Result.Cancelled = !CompletionWon;
      Result.ExitStatus = Status;
      return Result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
