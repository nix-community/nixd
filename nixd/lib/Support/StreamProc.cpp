#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "nixd/Support/StreamProc.h"
#include "ForkPipedInternal.h"
#include "nixd/Support/ForkPiped.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#if defined(__APPLE__)
#define NIXD_POSIX_SPAWN_HAS_WORKING_DIRECTORY
#elif defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 34)
#define NIXD_POSIX_SPAWN_HAS_WORKING_DIRECTORY
#endif
#endif

extern char **environ;

using namespace nixd;
using namespace util;
using namespace lspserver;

class nixd::detail::ProcessLaunchGuard {
  pid_t PID = -1;
  pid_t ProcessGroup = -1;

public:
  void arm(pid_t Child) noexcept { PID = Child; }

  void markValidatedProcessGroup(pid_t Group) noexcept {
    if (PID > 0 && Group == PID)
      ProcessGroup = Group;
  }

  void release() noexcept {
    PID = -1;
    ProcessGroup = -1;
  }

  ~ProcessLaunchGuard() {
    if (PID <= 0)
      return;

    siginfo_t Info{};
    int Observation;
    do {
      Observation = ::waitid(P_PID, PID, &Info, WEXITED | WNOHANG | WNOWAIT);
    } while (Observation < 0 && errno == EINTR);
    if (Observation < 0 && errno == ECHILD) {
      release();
      return;
    }

    const bool LeaderExited = Observation == 0 && Info.si_pid == PID;
    if (ProcessGroup == PID)
      (void)::kill(-ProcessGroup, SIGKILL);
    else if (!LeaderExited)
      (void)::kill(PID, SIGKILL);

    int Status = 0;
    while (::waitpid(PID, &Status, 0) < 0 && errno == EINTR) {
    }
    release();
  }
};

namespace {

struct LaunchedProcess {
  std::unique_ptr<nixd::detail::ProcessLaunchGuard> Guard;
  pid_t PID;
  pid_t ProcessGroup;
  AutoCloseFD Stdin;
  AutoCloseFD Stdout;

  LaunchedProcess(std::unique_ptr<nixd::detail::ProcessLaunchGuard> Guard,
                  pid_t PID, pid_t ProcessGroup, AutoCloseFD Stdin,
                  AutoCloseFD Stdout)
      : Guard(std::move(Guard)), PID(PID), ProcessGroup(ProcessGroup),
        Stdin(std::move(Stdin)), Stdout(std::move(Stdout)) {}
};

#ifdef NIXD_POSIX_SPAWN_HAS_WORKING_DIRECTORY
class SpawnFileActions {
  posix_spawn_file_actions_t Actions;

public:
  SpawnFileActions() {
    const int Error = ::posix_spawn_file_actions_init(&Actions);
    if (Error != 0)
      throw std::system_error(Error, std::generic_category());
  }

  ~SpawnFileActions() { (void)::posix_spawn_file_actions_destroy(&Actions); }

  posix_spawn_file_actions_t *get() { return &Actions; }
};

class SpawnAttributes {
  posix_spawnattr_t Attributes;

public:
  SpawnAttributes() {
    const int Error = ::posix_spawnattr_init(&Attributes);
    if (Error != 0)
      throw std::system_error(Error, std::generic_category());
  }

  ~SpawnAttributes() { (void)::posix_spawnattr_destroy(&Attributes); }

  posix_spawnattr_t *get() { return &Attributes; }
};

void checkSpawnAction(int Error) {
  if (Error != 0)
    throw std::system_error(Error, std::generic_category());
}

LaunchedProcess spawnWithFileActions(const ExecSpec &Spec,
                                     std::vector<char *> &Arguments) {
  auto [InRead, InWrite] = nixd::detail::openPipeCloseOnExec();
  auto [OutRead, OutWrite] = nixd::detail::openPipeCloseOnExec();

  SpawnFileActions Actions;
  checkSpawnAction(::posix_spawn_file_actions_adddup2(
      Actions.get(), InRead.get(), STDIN_FILENO));
  checkSpawnAction(::posix_spawn_file_actions_adddup2(
      Actions.get(), OutWrite.get(), STDOUT_FILENO));
  checkSpawnAction(::posix_spawn_file_actions_addopen(
      Actions.get(), STDERR_FILENO, Spec.Stderr.c_str(),
      O_WRONLY | O_CREAT | O_TRUNC, 0666));
  if (!Spec.WorkingDirectory.empty())
    checkSpawnAction(::posix_spawn_file_actions_addchdir_np(
        Actions.get(), Spec.WorkingDirectory.c_str()));
  for (int FD : {InRead.get(), InWrite.get(), OutRead.get(), OutWrite.get()})
    checkSpawnAction(::posix_spawn_file_actions_addclose(Actions.get(), FD));

  SpawnAttributes Attributes;
  checkSpawnAction(::posix_spawnattr_setpgroup(Attributes.get(), 0));
  short Flags = POSIX_SPAWN_SETPGROUP;
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
  Flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#else
  checkSpawnAction(
      ::posix_spawn_file_actions_addclosefrom_np(Actions.get(), 3));
#endif
  checkSpawnAction(::posix_spawnattr_setflags(Attributes.get(), Flags));

  auto ChildGuard = std::make_unique<nixd::detail::ProcessLaunchGuard>();
  pid_t Child = -1;
  const int Error =
      ::posix_spawn(&Child, Spec.Executable.c_str(), Actions.get(),
                    Attributes.get(), Arguments.data(), environ);
  if (Error != 0)
    throw std::system_error(Error, std::generic_category());
  ChildGuard->arm(Child);

  pid_t ProcessGroup;
  do {
    ProcessGroup = ::getpgid(Child);
  } while (ProcessGroup < 0 && errno == EINTR);
  if (ProcessGroup != Child) {
    const int Failure = ProcessGroup < 0 ? errno : EINVAL;
    throw std::system_error(Failure, std::generic_category());
  }
  ChildGuard->markValidatedProcessGroup(ProcessGroup);

  return LaunchedProcess(std::move(ChildGuard), Child, ProcessGroup,
                         std::move(InWrite), std::move(OutRead));
}
#endif

int duplicateDescriptor(int OldFD, int NewFD) noexcept {
  int Result;
  do {
    Result = ::dup2(OldFD, NewFD);
  } while (Result < 0 && errno == EINTR);
  return Result;
}

[[maybe_unused]] LaunchedProcess spawnWithFork(const ExecSpec &Spec,
                                               std::vector<char *> &Arguments) {
  auto [InRead, InWrite] = nixd::detail::openPipeCloseOnExec();
  auto [OutRead, OutWrite] = nixd::detail::openPipeCloseOnExec();

  int ErrorFD;
  do {
    ErrorFD = ::open(Spec.Stderr.c_str(),
                     O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  } while (ErrorFD < 0 && errno == EINTR);
  if (ErrorFD < 0)
    throw std::system_error(errno, std::generic_category());
  auto Error = nixd::detail::normalizePipeSource(AutoCloseFD(ErrorFD));

  const int DescriptorLimit = ::getdtablesize();
  const int ChildInRead = InRead.get();
  const int ChildOutWrite = OutWrite.get();
  const int ChildError = Error.get();
  const char *Executable = Spec.Executable.c_str();
  const char *WorkingDirectory =
      Spec.WorkingDirectory.empty() ? nullptr : Spec.WorkingDirectory.c_str();
  char *const *ArgumentData = Arguments.data();
  char **Environment = environ;
  auto ChildGuard = std::make_unique<nixd::detail::ProcessLaunchGuard>();
  const pid_t Child = ::fork();
  if (Child == 0) {
    (void)::setpgid(0, 0);
    if (duplicateDescriptor(ChildInRead, STDIN_FILENO) < 0 ||
        duplicateDescriptor(ChildOutWrite, STDOUT_FILENO) < 0 ||
        duplicateDescriptor(ChildError, STDERR_FILENO) < 0)
      _exit(126);
    for (int FD = STDERR_FILENO + 1; FD < DescriptorLimit; ++FD)
      (void)::close(FD);
    if (WorkingDirectory && ::chdir(WorkingDirectory) != 0)
      _exit(126);
    ::execve(Executable, ArgumentData, Environment);
    _exit(127);
  }
  if (Child < 0)
    throw std::system_error(errno, std::generic_category());
  ChildGuard->arm(Child);

  pid_t ProcessGroup = -1;
  if (::setpgid(Child, Child) == 0 || ::getpgid(Child) == Child)
    ProcessGroup = Child;
  if (ProcessGroup != Child) {
    const int Failure = errno ? errno : EINVAL;
    throw std::system_error(Failure, std::generic_category());
  }
  ChildGuard->markValidatedProcessGroup(ProcessGroup);

  return LaunchedProcess(std::move(ChildGuard), Child, ProcessGroup,
                         std::move(InWrite), std::move(OutRead));
}

LaunchedProcess spawnExec(const ExecSpec &Spec) {
  if (Spec.Executable.empty() || Spec.Arguments.empty())
    throw std::system_error(EINVAL, std::generic_category());

  std::vector<char *> Arguments;
  Arguments.reserve(Spec.Arguments.size() + 1);
  for (const auto &Argument : Spec.Arguments)
    Arguments.push_back(const_cast<char *>(Argument.c_str()));
  Arguments.push_back(nullptr);

  std::lock_guard Guard(nixd::detail::spawnWindowMutex());
#ifdef NIXD_POSIX_SPAWN_HAS_WORKING_DIRECTORY
  return spawnWithFileActions(Spec, Arguments);
#else
  return spawnWithFork(Spec, Arguments);
#endif
}

} // namespace

std::unique_ptr<InboundPort> StreamProc::mkIn() const {
  return std::make_unique<InboundPort>(Proc->Stdout.get(),
                                       JSONStreamStyle::Standard);
}

std::unique_ptr<OutboundPort> StreamProc::mkOut() const {
  return std::make_unique<OutboundPort>(*Stream);
}

StreamProc::StreamProc(const std::function<int()> &Action,
                       std::span<const int> ChildFDs) {
  int In;
  int Out;
  int Err;
  pid_t ProcessGroup = -1;
  auto Guard = std::make_unique<detail::ProcessLaunchGuard>();

  pid_t Child = forkPiped(In, Out, Err, &ProcessGroup, ChildFDs);
  if (Child == 0)
    _exit(Action());
  Guard->arm(Child);
  Guard->markValidatedProcessGroup(ProcessGroup);

  // Parent process.
  AutoCloseFD OwnedIn(In);
  AutoCloseFD OwnedOut(Out);
  AutoCloseFD OwnedErr(Err);
  auto NewProc = std::make_unique<PipedProc>(Child, ProcessGroup, OwnedIn.get(),
                                             OwnedOut.get(), OwnedErr.get());
  OwnedIn.release();
  OwnedOut.release();
  OwnedErr.release();
  auto NewStream =
      std::make_unique<llvm::raw_fd_ostream>(NewProc->Stdin.get(), false);
  ConstructionGuard = std::move(Guard);
  Proc = std::move(NewProc);
  Stream = std::move(NewStream);
}

StreamProc::StreamProc(const ExecSpec &Spec) {
  auto Launched = spawnExec(Spec);
  auto NewProc = std::make_unique<PipedProc>(
      Launched.PID, Launched.ProcessGroup, Launched.Stdin.get(),
      Launched.Stdout.get(), -1);
  Launched.Stdin.release();
  Launched.Stdout.release();
  auto NewStream =
      std::make_unique<llvm::raw_fd_ostream>(NewProc->Stdin.get(), false);
  ConstructionGuard = std::move(Launched.Guard);
  Proc = std::move(NewProc);
  Stream = std::move(NewStream);
}

StreamProc::~StreamProc() = default;

void StreamProc::claimProcess() noexcept {
  if (ConstructionGuard)
    ConstructionGuard->release();
  ConstructionGuard.reset();
}
