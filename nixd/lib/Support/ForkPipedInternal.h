#pragma once

#include "nixd/Support/AutoCloseFD.h"

#include <functional>
#include <mutex>
#include <span>
#include <sys/types.h>
#include <utility>

namespace nixd::detail {

struct ForkPipedSyscalls {
  std::function<int(int *)> Pipe;
  std::function<pid_t()> Fork;
  std::function<int(int, int)> Dup2;
};

std::mutex &spawnWindowMutex();
std::pair<util::AutoCloseFD, util::AutoCloseFD> openPipeCloseOnExec();
util::AutoCloseFD normalizePipeSource(util::AutoCloseFD FD);

int forkPipedWith(int &In, int &Out, int &Err, pid_t *ProcessGroup,
                  const ForkPipedSyscalls &Syscalls,
                  std::span<const int> ChildFDs = {});

} // namespace nixd::detail
