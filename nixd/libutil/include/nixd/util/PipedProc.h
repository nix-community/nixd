#pragma once

#include "AutoCloseFD.h"
#include "AutoHUPPID.h"

namespace nixd::util {

struct PipedProc {
  AutoHUPPID PID;

  // Piped descriptors
  AutoCloseFD Stdin;
  AutoCloseFD Stdout;
  AutoCloseFD Stderr;
};

} // namespace nixd::util
