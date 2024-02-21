#include "nixd/rpc/IO.h"

#include <unistd.h>

namespace nixd::rpc {

long readBytes(int FD, void *Buf, std::size_t N) {
  std::size_t Read = 0;
  while (Read < N) {
    auto BytesRead = read(FD, static_cast<char *>(Buf) + Read, N - Read);
    if (BytesRead <= 0)
      return BytesRead;
    Read += BytesRead;
  }
  return static_cast<long>(Read);
}

} // namespace nixd::rpc
