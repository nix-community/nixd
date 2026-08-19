#pragma once

#include <functional>
#include <sys/types.h>

namespace nixd::detail {

struct ForkPipedSyscalls {
  std::function<int(int *)> Pipe;
  std::function<pid_t()> Fork;
};

int forkPipedWith(int &In, int &Out, int &Err, pid_t *ProcessGroup,
                  const ForkPipedSyscalls &Syscalls);

} // namespace nixd::detail
