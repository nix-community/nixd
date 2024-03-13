#include <gtest/gtest.h>

#include "nixd/rpc/Protocol.h"
#include "nixd/rpc/Transport.h"

namespace {

using namespace nixd::rpc;

#define PIPE_READ 0
#define PIPE_WRITE 1

TEST(RPC, Message) {
  std::string Msg = "foo";

  int Pipes[2];
  ASSERT_EQ(pipe(Pipes), 0);

  send(Pipes[PIPE_WRITE], Msg);

  auto Result = recv(Pipes[PIPE_READ]);

  ASSERT_EQ(Result.size(), Msg.size());
}

TEST(RPC, MessageEval) {
  int Pipes[2];
  ASSERT_EQ(pipe(Pipes), 0);

  using DataT = Message<ExprValueParams>;

  sendPacket(Pipes[PIPE_WRITE],
             DataT{RPCKind::ExprValue, ExprValueParams{1, 2}});

  auto Msg = recvPacket<DataT>(Pipes[PIPE_READ]);

  ASSERT_EQ(Msg.Params.BCID, 1);
  ASSERT_EQ(Msg.Params.ExprID, 2);
}

} // namespace
