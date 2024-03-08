#include <csignal>

#include <sched.h>

namespace nixd::util {

class AutoHUPPID {
  pid_t Pid;

public:
  AutoHUPPID(pid_t Pid) noexcept : Pid(Pid) {}

  ~AutoHUPPID() { kill(Pid, SIGHUP); }
};

} // namespace nixd::util
