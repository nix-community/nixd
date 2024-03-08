#include "nixd/util/ForkPiped.h"

#include <cerrno>

#include <unistd.h>

namespace nixd::util {

int forkPiped(int &In, int &Out, int &Err) {
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  int PipeIn[2];
  int PipeOut[2];
  int PipeErr[2];
  if (pipe(PipeIn) == -1 || pipe(PipeOut) == -1 || pipe(PipeErr) == -1) {
    return -errno;
  }

  pid_t Child = fork();

  if (Child == 0) {
    // Redirect stdin, stdout, stderr.
    dup2(PipeIn[READ], STDIN_FILENO);
    dup2(PipeOut[WRITE], STDOUT_FILENO);
    dup2(PipeErr[WRITE], STDERR_FILENO);
    // Child process.
    return 0;
  }

  if (Child == -1) {
    // Error.
    return -errno;
  }

  In = PipeIn[WRITE];
  Out = PipeOut[READ];
  Err = PipeOut[READ];
  return Child;
}

} // namespace nixd::util
