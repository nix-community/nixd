#pragma once

#include "nixd/rpc/Protocol.h"

#include <bc/Write.h>

#include <string_view>
#include <vector>

namespace nixd::rpc {

class Transport {
  int InboundFD;
  int OutboundFD;

protected:
  /// \brief Wraps a message with it's length, and then send it.
  void send(std::string_view Msg) const;

  /// \brief Read a message and then forms a packet.
  [[nodiscard]] std::vector<char> recv() const;

  template <class T> T recvPacket() const {
    std::vector<char> Buf = recv();
    T Data;
    std::string_view D = {Buf.begin(), Buf.end()};
    readBytecode(D, Data);
    return Data;
  }

  template <class T> void sendPacket(const T &Data) {
    auto Str = bc::toBytecode(Data);
    send(Str);
  }

  virtual void handleInbound(const std::vector<char> &Buf) = 0;

public:
  [[nodiscard]] int run();
  Transport(int InboundFD, int OutboundFD)
      : InboundFD(InboundFD), OutboundFD(OutboundFD) {}
};

} // namespace nixd::rpc
