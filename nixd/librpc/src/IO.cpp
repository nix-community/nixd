#include "nixd/rpc/IO.h"

#include <system_error>

#include <unistd.h>

namespace nixd::rpc {

size_t readBytes(int FD, void *Buf, std::size_t N) {
  size_t Read = 0;
  while (Read < N) {
    size_t BytesRead = read(FD, static_cast<char *>(Buf) + Read, N - Read);
    if (BytesRead < 0)
      throw std::system_error(std::make_error_code(std::errc(errno)));
    if (BytesRead == 0)
      break;
    Read += BytesRead;
  }
  return Read;
}

} // namespace nixd::rpc
