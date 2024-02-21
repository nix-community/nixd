#pragma once

#include <string_view>
#include <vector>

namespace nixd::rpc {

class Transport {
  int InboundFD;
  int OutboundFD;

protected:
  [[nodiscard]] int send(std::string_view Msg) const;

  [[nodiscard]] virtual int handleInbound(const std::vector<char> &Buf) = 0;

public:
  [[nodiscard]] int run();
  Transport(int InboundFD, int OutboundFD)
      : InboundFD(InboundFD), OutboundFD(OutboundFD) {}
};

} // namespace nixd::rpc
