#pragma once

#include <fcntl.h>
#include <mutex>
#include <unistd.h>

namespace nixd::detail {

// Keep this scope alive from the first pipe creation through fork. Callers own
// and close all pipe descriptors, including those left by a failed createPipe.
#if defined(__linux__)
class PipeForkScope {
public:
  int createPipe(int Pipe[2]) const { return pipe2(Pipe, O_CLOEXEC); }
  pid_t forkProcess() { return fork(); }
};
#else
// macOS has no pipe2. Serialize pipe creation and fork so another caller cannot
// inherit a descriptor between pipe() and fcntl(). All concurrent process
// creation must use this scope for that guarantee to hold.
class PipeForkScope {
  static std::mutex &forkMutex() {
    static std::mutex Mutex;
    return Mutex;
  }

  std::unique_lock<std::mutex> Lock{forkMutex()};

public:
  int createPipe(int Pipe[2]) const {
    if (pipe(Pipe) == -1)
      return -1;
    for (int I = 0; I < 2; ++I)
      if (fcntl(Pipe[I], F_SETFD, FD_CLOEXEC) == -1)
        return -1;
    return 0;
  }

  pid_t forkProcess() {
    const pid_t Child = fork();
    // Only the parent may unlock the mutex. The child must exec or _exit
    // without creating another PipeForkScope.
    if (Child == 0)
      Lock.release();
    return Child;
  }
};
#endif

} // namespace nixd::detail
