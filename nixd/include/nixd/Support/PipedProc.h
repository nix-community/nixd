#pragma once

#include "AutoCloseFD.h"

#include <sys/types.h>

namespace nixd::util {

struct PipedProc {
  pid_t PID;
  /// Dedicated, validated child process group, or -1 when unavailable.
  pid_t ProcessGroup;

  // Piped descriptors
  AutoCloseFD Stdin;
  AutoCloseFD Stdout;
  AutoCloseFD Stderr;

  PipedProc(pid_t PID, pid_t ProcessGroup, int In, int Out, int Err)
      : PID(PID), ProcessGroup(ProcessGroup), Stdin(In), Stdout(Out),
        Stderr(Err) {}
  PipedProc(PipedProc &&) noexcept = default;
  PipedProc(const PipedProc &) = delete;
};

} // namespace nixd::util
