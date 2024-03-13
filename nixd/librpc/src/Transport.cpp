#include "nixd/rpc/Transport.h"
#include "nixd/rpc/IO.h"

#include <stdexcept>
#include <system_error>
#include <vector>

#include <unistd.h>

namespace nixd::rpc {

using MessageSizeT = std::size_t;

void send(int OutboundFD, std::string_view Msg) {
  MessageSizeT Size = Msg.size();
  if (write(OutboundFD, &Size, sizeof(MessageSizeT)) < 0)
    throw std::system_error(std::make_error_code(std::errc(errno)));
  if (write(OutboundFD, Msg.data(), Msg.size()) < 0)
    throw std::system_error(std::make_error_code(std::errc(errno)));
}

std::vector<char> recv(int InboundFD) {
  MessageSizeT Size;
  if (readBytes(InboundFD, &Size, sizeof(MessageSizeT)) <
      sizeof(MessageSizeT)) {
    return {};
  }

  // Collect a message
  std::vector<char> Buf(Size);
  if (readBytes(InboundFD, Buf.data(), Size) < Size) {
    throw std::runtime_error("in-complete inbound size");
  }
  return Buf;
}

int Transport::run() {
  for (;;) {
    std::vector<char> Buf = recv();
    if (Buf.empty())
      break;
    handleInbound(Buf);
  }
  return 0;
}

} // namespace nixd::rpc
