#include "nixd/Support/ForkPiped.h"

#include <cerrno>

#include <system_error>
#include <unistd.h>

int nixd::forkPiped(int &In, int &Out, int &Err) {
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
  if (pipe(PipeIn) == -1 || pipe(PipeOut) == -1 || pipe(PipeErr) == -1)
    Fail();

  pid_t Child = fork();
  if (Child == -1)
    Fail();

  if (Child == 0) {
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

  close(PipeIn[READ]);
  close(PipeOut[WRITE]);
  close(PipeErr[WRITE]);

  In = PipeIn[WRITE];
  Out = PipeOut[READ];
  Err = PipeErr[READ];
  return Child;
}
