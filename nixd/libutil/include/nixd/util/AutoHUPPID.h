#include <csignal>

#include <sched.h>

namespace nixd::util {

class AutoHUPPID {
  pid_t Pid;

public:
  AutoHUPPID(pid_t Pid) noexcept : Pid(Pid) {}

  ~AutoHUPPID() { kill(Pid, SIGKILL); }

  operator pid_t() const { return Pid; }
};

} // namespace nixd::util
