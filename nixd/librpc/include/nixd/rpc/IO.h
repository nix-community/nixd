#pragma once

#include <cstdint>
#include <string>

namespace nixd::rpc {

/// \brief Read N bytes from FD into Buf, block until N bytes are read.
/// \returns the number of bytes read, or -1 on error. (errno is set)
long readBytes(int FD, void *Buf, std::size_t N);

} // namespace nixd::rpc
