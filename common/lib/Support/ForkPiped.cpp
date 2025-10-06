#include "nixd/Support/ForkPiped.h"

#include <cerrno>

#include <system_error>
#include <unistd.h>

int nixd::forkPiped(int &In, int &Out, int &Err) {
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  int PipeIn[2];
  int PipeOut[2];
  int PipeErr[2];
  if (pipe(PipeIn) == -1 || pipe(PipeOut) == -1 || pipe(PipeErr) == -1)
    throw std::system_error(errno, std::generic_category());

  pid_t Child = fork();

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

  if (Child == -1)
    throw std::system_error(errno, std::generic_category());

  In = PipeIn[WRITE];
  Out = PipeOut[READ];
  Err = PipeOut[READ];
  return Child;
}
