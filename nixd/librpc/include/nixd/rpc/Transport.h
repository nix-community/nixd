#pragma once

#include "nixd/rpc/Protocol.h"

#include <bc/Write.h>

#include <string_view>
#include <vector>

namespace nixd::rpc {

/// \brief Wraps a message with it's length, and then send it.
void send(int OutboundFD, std::string_view Msg);

/// \brief Wraps a message with it's length, and then send it.
std::vector<char> recv(int InboundFD);

template <class T> T recvPacket(int InboundFD) {
  std::vector<char> Buf = recv(InboundFD);
  T Data;
  std::string_view D = {Buf.begin(), Buf.end()};
  readBytecode(D, Data);
  return Data;
}

template <class T> void sendPacket(int OutboundFD, const T &Data) {
  auto Str = bc::toBytecode(Data);
  send(OutboundFD, Str);
}

class Transport {
  int InboundFD;
  int OutboundFD;

protected:
  /// \brief Wraps a message with it's length, and then send it.
  void send(std::string_view Msg) const { rpc::send(OutboundFD, Msg); }

  /// \brief Wraps a message with it's length, and then send it.
  [[nodiscard]] std::vector<char> recv() const { return rpc::recv(InboundFD); }

  template <class T> T recvPacket() { return rpc::recvPacket<T>(InboundFD); }

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
