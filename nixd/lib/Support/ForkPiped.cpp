#include "nixd/Support/ForkPiped.h"

#include "ForkPipedPlatform.h"

#include <cerrno>
#include <fcntl.h>

#include <system_error>
#include <unistd.h>
#include <utility>

namespace {

// Own every endpoint until it is transferred to the parent. This object is
// constructed before pipe creation so partial creation failures are covered.
struct PipeDescriptors {
  int In[2] = {-1, -1};
  int Out[2] = {-1, -1};
  int Err[2] = {-1, -1};

  PipeDescriptors() = default;
  PipeDescriptors(const PipeDescriptors &) = delete;
  PipeDescriptors &operator=(const PipeDescriptors &) = delete;

  ~PipeDescriptors() {
    for (const auto *Pipe : {In, Out, Err})
      for (int I = 0; I < 2; ++I)
        if (Pipe[I] != -1)
          close(Pipe[I]);
  }
};

} // namespace

int nixd::forkPiped(int &In, int &Out, int &Err) {
  detail::PipeForkScope Platform;
  static constexpr int READ = 0;
  static constexpr int WRITE = 1;
  PipeDescriptors Pipes;
  auto &PipeIn = Pipes.In;
  auto &PipeOut = Pipes.Out;
  auto &PipeErr = Pipes.Err;
  if (Platform.createPipe(PipeIn) == -1 || Platform.createPipe(PipeOut) == -1 ||
      Platform.createPipe(PipeErr) == -1)
    throw std::system_error(errno, std::generic_category());

  // Keep the sources away from stdio, even if the caller closed fd 0, 1 or 2.
  // This also ensures dup2 clears CLOEXEC rather than becoming a no-op.
  for (auto *Pipe : {PipeIn, PipeOut, PipeErr})
    for (int I = 0; I < 2; ++I)
      if (Pipe[I] <= STDERR_FILENO) {
        int FD = fcntl(Pipe[I], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        if (FD == -1)
          throw std::system_error(errno, std::generic_category());
        close(Pipe[I]);
        Pipe[I] = FD;
      }

  pid_t Child = Platform.forkProcess();
  if (Child == -1)
    throw std::system_error(errno, std::generic_category());

  if (Child == 0) {
    // Redirect stdin, stdout, stderr.
    const int Sources[] = {PipeIn[READ], PipeOut[WRITE], PipeErr[WRITE]};
    for (int Target = STDIN_FILENO; Target <= STDERR_FILENO; ++Target) {
      int Result;
      do {
        Result = dup2(Sources[Target], Target);
      } while (Result == -1 && errno == EINTR);
      if (Result == -1)
        _exit(127);
    }
    return 0;
  }

  In = std::exchange(PipeIn[WRITE], -1);
  Out = std::exchange(PipeOut[READ], -1);
  Err = std::exchange(PipeErr[READ], -1);
  return Child;
}
