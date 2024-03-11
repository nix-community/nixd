#pragma once

#include <cstddef>

#include <sys/types.h>

namespace nixd::rpc {

/// \brief Read N bytes from FD into Buf, block until N bytes are read.
size_t readBytes(int FD, void *Buf, std::size_t N);

} // namespace nixd::rpc
