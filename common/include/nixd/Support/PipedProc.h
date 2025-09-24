#pragma once

#include "AutoCloseFD.h"

#include <sys/types.h>

namespace nixd::util {

struct PipedProc {
  pid_t PID;

  // Piped descriptors
  AutoCloseFD Stdin;
  AutoCloseFD Stdout;
  AutoCloseFD Stderr;
};

} // namespace nixd::util
