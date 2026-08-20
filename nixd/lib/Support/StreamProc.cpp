#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "nixd/Support/StreamProc.h"
#include "ForkPipedInternal.h"
#include "nixd/Support/ForkPiped.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 34)
#define NIXD_POSIX_SPAWN_HAS_CLOSEFROM_NP
#endif
#endif

extern char **environ;

using namespace nixd;
using namespace util;
using namespace lspserver;

namespace {

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

void terminateUnvalidatedChild(pid_t Child) noexcept {
  (void)::kill(Child, SIGKILL);
  int Status = 0;
  while (::waitpid(Child, &Status, 0) < 0 && errno == EINTR) {
  }
}

std::unique_ptr<PipedProc> spawnExec(const ExecSpec &Spec) {
  if (Spec.Executable.empty() || Spec.Arguments.empty())
    throw std::system_error(EINVAL, std::generic_category());

  std::lock_guard Guard(nixd::detail::spawnWindowMutex());
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
  for (int FD : {InRead.get(), InWrite.get(), OutRead.get(), OutWrite.get()})
    checkSpawnAction(::posix_spawn_file_actions_addclose(Actions.get(), FD));

  SpawnAttributes Attributes;
  checkSpawnAction(::posix_spawnattr_setpgroup(Attributes.get(), 0));
  short Flags = POSIX_SPAWN_SETPGROUP;
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
  Flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#elif defined(NIXD_POSIX_SPAWN_HAS_CLOSEFROM_NP)
  checkSpawnAction(
      ::posix_spawn_file_actions_addclosefrom_np(Actions.get(), 3));
#else
  const std::array PipeFDs{InRead.get(), InWrite.get(), OutRead.get(),
                           OutWrite.get()};
  for (int FD = STDERR_FILENO + 1; FD < ::getdtablesize(); ++FD) {
    if (std::ranges::find(PipeFDs, FD) != PipeFDs.end())
      continue;
    int DescriptorFlags;
    do {
      DescriptorFlags = ::fcntl(FD, F_GETFD);
    } while (DescriptorFlags < 0 && errno == EINTR);
    if (DescriptorFlags < 0 && errno == EBADF)
      continue;
    checkSpawnAction(::posix_spawn_file_actions_addclose(Actions.get(), FD));
  }
#endif
  checkSpawnAction(::posix_spawnattr_setflags(Attributes.get(), Flags));

  std::vector<char *> Arguments;
  Arguments.reserve(Spec.Arguments.size() + 1);
  for (const auto &Argument : Spec.Arguments)
    Arguments.push_back(const_cast<char *>(Argument.c_str()));
  Arguments.push_back(nullptr);

  pid_t Child = -1;
  const int Error =
      ::posix_spawn(&Child, Spec.Executable.c_str(), Actions.get(),
                    Attributes.get(), Arguments.data(), environ);
  if (Error != 0)
    throw std::system_error(Error, std::generic_category());

  pid_t ProcessGroup;
  do {
    ProcessGroup = ::getpgid(Child);
  } while (ProcessGroup < 0 && errno == EINTR);
  if (ProcessGroup != Child) {
    const int Failure = ProcessGroup < 0 ? errno : EINVAL;
    terminateUnvalidatedChild(Child);
    throw std::system_error(Failure, std::generic_category());
  }

  auto Result = std::make_unique<PipedProc>(Child, ProcessGroup, InWrite.get(),
                                            OutRead.get(), -1);
  InWrite.release();
  OutRead.release();
  return Result;
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

  pid_t Child = forkPiped(In, Out, Err, &ProcessGroup, ChildFDs);
  if (Child == 0)
    _exit(Action());

  // Parent process.
  Proc = std::make_unique<PipedProc>(Child, ProcessGroup, In, Out, Err);
  Stream = std::make_unique<llvm::raw_fd_ostream>(In, false);
}

StreamProc::StreamProc(const ExecSpec &Spec) : Proc(spawnExec(Spec)) {
  Stream = std::make_unique<llvm::raw_fd_ostream>(Proc->Stdin.get(), false);
}
