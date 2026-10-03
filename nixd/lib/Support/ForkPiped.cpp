#include "nixd/Support/ForkPiped.h"

#include <nix/util/file-descriptor.hh>

#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <system_error>
#include <unistd.h>

int nixd::forkPiped(int &In, int &Out, int &Err) {
#if !defined(__linux__)
  // macOS has no pipe2: prevent another forkPiped call from forking between
  // pipe() and fcntl(). All process creation must use this helper for this
  // guarantee to hold.
  static std::mutex ForkMutex;
  std::unique_lock ForkLock(ForkMutex);
#endif
  nix::Pipe PipeIn, PipeOut, PipeErr;
  PipeIn.create();
  PipeOut.create();
  PipeErr.create();

  // Keep the sources away from stdio, even if the caller closed fd 0, 1 or 2.
  // This also ensures dup2 clears CLOEXEC rather than becoming a no-op.
  for (auto *Pipe : {&PipeIn, &PipeOut, &PipeErr})
    for (auto *End : {&Pipe->readSide, &Pipe->writeSide})
      if (End->get() <= STDERR_FILENO) {
        int FD = fcntl(End->get(), F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        if (FD == -1)
          throw std::system_error(errno, std::generic_category());
        *End = nix::AutoCloseFD(FD);
      }

  pid_t Child = fork();
  if (Child == -1)
    throw std::system_error(errno, std::generic_category());

  if (Child == 0) {
#if !defined(__linux__)
    // The inherited mutex belongs to the parent. Do not unlock it in the
    // child; callers must exec or _exit without using this helper again.
    ForkLock.release();
#endif
    // Redirect stdin, stdout, stderr.
    // Release ownership before using raw syscalls in the child.
    close(PipeIn.writeSide.release());
    close(PipeOut.readSide.release());
    close(PipeErr.readSide.release());
    const int Sources[] = {PipeIn.readSide.release(),
                           PipeOut.writeSide.release(),
                           PipeErr.writeSide.release()};
    for (int Target = STDIN_FILENO; Target <= STDERR_FILENO; ++Target) {
      int Result;
      do {
        Result = dup2(Sources[Target], Target);
      } while (Result == -1 && errno == EINTR);
      if (Result == -1)
        _exit(127);
    }
    for (int FD : Sources)
      close(FD);
    // Child process.
    return 0;
  }

  In = PipeIn.writeSide.release();
  Out = PipeOut.readSide.release();
  Err = PipeErr.readSide.release();
  return Child;
}
