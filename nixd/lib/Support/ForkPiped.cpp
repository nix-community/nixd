#include "nixd/Support/ForkPiped.h"
#include "ForkPipedInternal.h"
#include "nixd/Support/AutoCloseFD.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <mutex>

#include <system_error>
#include <utility>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <unistd.h>

namespace {

int pipeCloseOnExec(int *FDs) {
#ifdef __linux__
  return ::pipe2(FDs, O_CLOEXEC);
#else
  if (::pipe(FDs) < 0)
    return -1;
  if (::fcntl(FDs[0], F_SETFD, FD_CLOEXEC) == 0 &&
      ::fcntl(FDs[1], F_SETFD, FD_CLOEXEC) == 0)
    return 0;
  const int Failure = errno;
  (void)::close(FDs[0]);
  (void)::close(FDs[1]);
  errno = Failure;
  return -1;
#endif
}

void closeDescriptorRange(unsigned First, unsigned Last,
                          int DescriptorLimit) noexcept {
  if (First > Last)
    return;
#if defined(__linux__) && defined(SYS_close_range)
  if (::syscall(SYS_close_range, First, Last, 0) == 0)
    return;
#endif
  const unsigned BoundedLast =
      std::min(Last, static_cast<unsigned>(DescriptorLimit - 1));
  for (unsigned FD = First; FD <= BoundedLast; ++FD)
    (void)::close(static_cast<int>(FD));
}

void closeUnownedChildDescriptors(std::span<const int> ChildFDs,
                                  int DescriptorLimit) noexcept {
  if (DescriptorLimit <= STDERR_FILENO + 1)
    return;
  const int SavedErrno = errno;
  unsigned First = STDERR_FILENO + 1;
  const unsigned End = static_cast<unsigned>(DescriptorLimit);
  while (First < End) {
    unsigned NextKept = End;
    for (int FD : ChildFDs) {
      if (FD >= static_cast<int>(First) && static_cast<unsigned>(FD) < NextKept)
        NextKept = static_cast<unsigned>(FD);
    }
    if (First < NextKept)
      closeDescriptorRange(First, NextKept - 1, DescriptorLimit);
    if (NextKept == End)
      break;
    First = NextKept + 1;
  }
  errno = SavedErrno;
}

nixd::util::AutoCloseFD normalizePipeSource(nixd::util::AutoCloseFD FD) {
  if (FD.get() > STDERR_FILENO)
    return FD;

  int Normalized;
  do {
    Normalized = ::fcntl(FD.get(), F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
  } while (Normalized < 0 && errno == EINTR);
  if (Normalized < 0) {
    const int Failure = errno;
    throw std::system_error(Failure, std::generic_category());
  }
  return nixd::util::AutoCloseFD(Normalized);
}

} // namespace

int nixd::detail::forkPipedWith(int &In, int &Out, int &Err,
                                pid_t *ProcessGroup,
                                const ForkPipedSyscalls &Syscalls,
                                std::span<const int> ChildFDs) {
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  const auto OpenPipe = [&] {
    int FDs[2];
    if (Syscalls.Pipe(FDs) == -1) {
      const int Failure = errno;
      throw std::system_error(Failure, std::generic_category());
    }
    util::AutoCloseFD RawRead(FDs[READ]);
    util::AutoCloseFD RawWrite(FDs[WRITE]);
    auto Read = normalizePipeSource(std::move(RawRead));
    auto Write = normalizePipeSource(std::move(RawWrite));
    return std::pair(std::move(Read), std::move(Write));
  };
  auto [InRead, InWrite] = OpenPipe();
  auto [OutRead, OutWrite] = OpenPipe();
  auto [ErrRead, ErrWrite] = OpenPipe();

  const int DescriptorLimit = ::getdtablesize();
  pid_t Child = Syscalls.Fork();

  if (Child == 0) {
    const int ChildInRead = InRead.get();
    const int ChildInWrite = InWrite.get();
    const int ChildOutRead = OutRead.get();
    const int ChildOutWrite = OutWrite.get();
    const int ChildErrRead = ErrRead.get();
    const int ChildErrWrite = ErrWrite.get();
    // The child retains raw descriptor values for close/dup2 below. Neutralize
    // every inherited scope guard first so returning from this function cannot
    // double-close an fd number that has since been reused.
    InRead.release();
    InWrite.release();
    OutRead.release();
    OutWrite.release();
    ErrRead.release();
    ErrWrite.release();
    // A dedicated group lets the owning process terminate descendants that
    // retain inherited pipe writers. The parent independently validates it.
    (void)setpgid(0, 0);
    // Redirect stdin, stdout, stderr.
    close(ChildInWrite);
    close(ChildOutRead);
    close(ChildErrRead);
    const auto DuplicateTo = [&](int OldFD, int NewFD) {
      int Result;
      do {
        Result = Syscalls.Dup2(OldFD, NewFD);
      } while (Result < 0 && errno == EINTR);
      return Result;
    };
    if (DuplicateTo(ChildInRead, STDIN_FILENO) < 0 ||
        DuplicateTo(ChildOutWrite, STDOUT_FILENO) < 0 ||
        DuplicateTo(ChildErrWrite, STDERR_FILENO) < 0)
      _exit(126);
    close(ChildInRead);
    close(ChildOutWrite);
    close(ChildErrWrite);
    // CLOEXEC protects exec children. StreamProc actions may remain in-process,
    // so close every unrelated inherited descriptor before returning to them;
    // tests that need a coordination descriptor opt in through ChildFDs.
    closeUnownedChildDescriptors(ChildFDs, DescriptorLimit);
    // Child process.
    return 0;
  }

  if (Child == -1) {
    const int Failure = errno;
    throw std::system_error(Failure, std::generic_category());
  }

  if (ProcessGroup) {
    *ProcessGroup = -1;
    if (::setpgid(Child, Child) == 0 || ::getpgid(Child) == Child)
      *ProcessGroup = Child;
  }

  In = InWrite.get();
  Out = OutRead.get();
  Err = ErrRead.get();
  InWrite.release();
  OutRead.release();
  ErrRead.release();
  return Child;
}

int nixd::forkPiped(int &In, int &Out, int &Err, pid_t *ProcessGroup,
                    std::span<const int> ChildFDs) {
  // Darwin lacks pipe2(O_CLOEXEC). Serializing every production forkPiped call
  // keeps its pipe+fcntl window closed to other children created here. Linux
  // uses atomic pipe2 as defense in depth and participates in the same window.
  static std::mutex ForkMutex;
  std::lock_guard Guard(ForkMutex);
  detail::ForkPipedSyscalls Syscalls{
      .Pipe = [](int *FDs) { return pipeCloseOnExec(FDs); },
      .Fork = [] { return ::fork(); },
      .Dup2 = [](int OldFD, int NewFD) { return ::dup2(OldFD, NewFD); },
  };
  return detail::forkPipedWith(In, Out, Err, ProcessGroup, Syscalls, ChildFDs);
}
