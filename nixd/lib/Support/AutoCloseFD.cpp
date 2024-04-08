#include "nixd/Support/AutoCloseFD.h"

#include <unistd.h>

namespace nixd::util {

AutoCloseFD::~AutoCloseFD() {
  if (FD != ReleasedFD) [[likely]]
    close(FD);
}

AutoCloseFD::AutoCloseFD(AutoCloseFD &&That) noexcept : FD(That.get()) {
  That.release();
}

AutoCloseFD::FDTy AutoCloseFD::get() const { return FD; }

void AutoCloseFD::release() { FD = ReleasedFD; }

bool AutoCloseFD::isReleased(FDTy FD) { return FD == ReleasedFD; }

bool AutoCloseFD::isReleased() const { return isReleased(FD); }

AutoCloseFD::AutoCloseFD(FDTy FD) : FD(FD) {}

} // namespace nixd::util
