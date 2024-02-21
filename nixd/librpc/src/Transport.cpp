#include "nixd/rpc/Transport.h"
#include "nixd/rpc/IO.h"

#include <unistd.h>
#include <vector>

namespace nixd::rpc {

using MessageSizeT = std::size_t;

int Transport::send(std::string_view Msg) const {
  MessageSizeT Size = Msg.size();
  if (write(OutboundFD, &Size, sizeof(MessageSizeT)) < 0)
    return -1;
  if (write(OutboundFD, Msg.data(), Msg.size()) < 0)
    return -1;
  return 0;
}

int Transport::run() {
  for (;;) {
    MessageSizeT Size;
    if (readBytes(InboundFD, &Size, sizeof(Size)) <= 0) {
      // Handle error
    }

    // Then read the message
    // Collect a message
    std::vector<char> Buf(Size);
    if (readBytes(InboundFD, Buf.data(), Size) <= 0)
      return -1;
    if (handleInbound(Buf) != 0)
      return -1;
  }
  return 0;
}

} // namespace nixd::rpc
