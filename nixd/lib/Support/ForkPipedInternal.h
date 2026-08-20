#pragma once

#include <functional>
#include <span>
#include <sys/types.h>

namespace nixd::detail {

struct ForkPipedSyscalls {
  std::function<int(int *)> Pipe;
  std::function<pid_t()> Fork;
};

int forkPipedWith(int &In, int &Out, int &Err, pid_t *ProcessGroup,
                  const ForkPipedSyscalls &Syscalls,
                  std::span<const int> ChildFDs = {});

} // namespace nixd::detail
