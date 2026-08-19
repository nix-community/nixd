#include "nixd/Support/ForkPiped.h"
#include "ForkPipedInternal.h"
#include "nixd/Support/AutoCloseFD.h"

#include <cerrno>

#include <system_error>
#include <unistd.h>

int nixd::detail::forkPipedWith(int &In, int &Out, int &Err,
                                pid_t *ProcessGroup,
                                const ForkPipedSyscalls &Syscalls) {
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  int PipeIn[2];
  int PipeOut[2];
  int PipeErr[2];
  if (Syscalls.Pipe(PipeIn) == -1)
    throw std::system_error(errno, std::generic_category());
  util::AutoCloseFD InRead(PipeIn[READ]);
  util::AutoCloseFD InWrite(PipeIn[WRITE]);
  if (Syscalls.Pipe(PipeOut) == -1) {
    const int Failure = errno;
    throw std::system_error(Failure, std::generic_category());
  }
  util::AutoCloseFD OutRead(PipeOut[READ]);
  util::AutoCloseFD OutWrite(PipeOut[WRITE]);
  if (Syscalls.Pipe(PipeErr) == -1) {
    const int Failure = errno;
    throw std::system_error(Failure, std::generic_category());
  }
  util::AutoCloseFD ErrRead(PipeErr[READ]);
  util::AutoCloseFD ErrWrite(PipeErr[WRITE]);

  pid_t Child = Syscalls.Fork();

  if (Child == 0) {
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
    close(PipeIn[WRITE]);
    close(PipeOut[READ]);
    close(PipeErr[READ]);
    dup2(PipeIn[READ], STDIN_FILENO);
    dup2(PipeOut[WRITE], STDOUT_FILENO);
    dup2(PipeErr[WRITE], STDERR_FILENO);
    close(PipeIn[READ]);
    close(PipeOut[WRITE]);
    close(PipeErr[WRITE]);
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

int nixd::forkPiped(int &In, int &Out, int &Err, pid_t *ProcessGroup) {
  detail::ForkPipedSyscalls Syscalls{
      .Pipe = [](int *FDs) { return ::pipe(FDs); },
      .Fork = [] { return ::fork(); },
  };
  return detail::forkPipedWith(In, Out, Err, ProcessGroup, Syscalls);
}
