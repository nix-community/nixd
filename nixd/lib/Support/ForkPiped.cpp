#include "nixd/Support/ForkPiped.h"

#include "ForkPipedPlatform.h"

#include <cerrno>
#include <fcntl.h>

#include <system_error>
#include <unistd.h>

int nixd::forkPiped(int &In, int &Out, int &Err) {
  detail::PipeForkScope Platform;
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  int PipeIn[2] = {-1, -1};
  int PipeOut[2] = {-1, -1};
  int PipeErr[2] = {-1, -1};
  auto CleanupAndMakeError = [&] {
    const int Error = errno;
    for (const auto *Pipe : {PipeIn, PipeOut, PipeErr})
      for (int I = 0; I < 2; ++I)
        if (Pipe[I] != -1)
          close(Pipe[I]);
    return std::system_error(Error, std::generic_category());
  };
  if (Platform.createPipe(PipeIn) == -1 || Platform.createPipe(PipeOut) == -1 ||
      Platform.createPipe(PipeErr) == -1)
    throw CleanupAndMakeError();

  // Keep the sources away from stdio, even if the caller closed fd 0, 1 or 2.
  // This also ensures dup2 clears CLOEXEC rather than becoming a no-op.
  for (auto *Pipe : {PipeIn, PipeOut, PipeErr})
    for (int I = 0; I < 2; ++I)
      if (Pipe[I] <= STDERR_FILENO) {
        int FD = fcntl(Pipe[I], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        if (FD == -1)
          throw CleanupAndMakeError();
        close(Pipe[I]);
        Pipe[I] = FD;
      }

  pid_t Child = Platform.forkProcess();
  if (Child == -1)
    throw CleanupAndMakeError();

  if (Child == 0) {
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
