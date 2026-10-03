#include "nixd/Support/ForkPiped.h"

#include <cerrno>
#include <fcntl.h>
#if !defined(__linux__)
#include <mutex>
#endif

#include <system_error>
#include <unistd.h>

namespace {

int pipeCloexec(int Pipe[2]) {
#if defined(__linux__)
  return pipe2(Pipe, O_CLOEXEC);
#else
  if (pipe(Pipe) == -1)
    return -1;
  for (int I = 0; I < 2; ++I)
    if (fcntl(Pipe[I], F_SETFD, FD_CLOEXEC) == -1)
      return -1;
  return 0;
#endif
}

} // namespace

int nixd::forkPiped(int &In, int &Out, int &Err) {
#if !defined(__linux__)
  // macOS has no pipe2: prevent another forkPiped call from forking between
  // pipe() and fcntl(). All process creation must use this helper for this
  // guarantee to hold.
  static std::mutex ForkMutex;
  std::unique_lock ForkLock(ForkMutex);
#endif
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  int PipeIn[2] = {-1, -1};
  int PipeOut[2] = {-1, -1};
  int PipeErr[2] = {-1, -1};
  auto Fail = [&] {
    const int Error = errno;
    for (const auto *Pipe : {PipeIn, PipeOut, PipeErr})
      for (int I = 0; I < 2; ++I)
        if (Pipe[I] != -1)
          close(Pipe[I]);
    throw std::system_error(Error, std::generic_category());
  };
  if (pipeCloexec(PipeIn) == -1 || pipeCloexec(PipeOut) == -1 ||
      pipeCloexec(PipeErr) == -1)
    Fail();

  // Keep the sources away from stdio, even if the caller closed fd 0, 1 or 2.
  // This also ensures dup2 clears CLOEXEC rather than becoming a no-op.
  for (auto *Pipe : {PipeIn, PipeOut, PipeErr})
    for (int I = 0; I < 2; ++I)
      if (Pipe[I] <= STDERR_FILENO) {
        int FD = fcntl(Pipe[I], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        if (FD == -1)
          Fail();
        close(Pipe[I]);
        Pipe[I] = FD;
      }

  pid_t Child = fork();
  if (Child == -1)
    Fail();

  if (Child == 0) {
#if !defined(__linux__)
    // The inherited mutex belongs to the parent. Do not unlock it in the
    // child; callers must exec or _exit without using this helper again.
    ForkLock.release();
#endif
    // Redirect stdin, stdout, stderr.
    close(PipeIn[WRITE]);
    close(PipeOut[READ]);
    close(PipeErr[READ]);
    const int Sources[] = {PipeIn[READ], PipeOut[WRITE], PipeErr[WRITE]};
    for (int Target = STDIN_FILENO; Target <= STDERR_FILENO; ++Target) {
      int Result;
      do {
        Result = dup2(Sources[Target], Target);
      } while (Result == -1 && errno == EINTR);
      if (Result == -1)
        _exit(127);
    }
    close(PipeIn[READ]);
    close(PipeOut[WRITE]);
    close(PipeErr[WRITE]);
    // Child process.
    return 0;
  }

  close(PipeIn[READ]);
  close(PipeOut[WRITE]);
  close(PipeErr[WRITE]);

  In = PipeIn[WRITE];
  Out = PipeOut[READ];
  Err = PipeErr[READ];
  return Child;
}
