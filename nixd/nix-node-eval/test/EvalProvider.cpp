#include <gtest/gtest.h>

#include "EvalProvider.h"

#include "nixd/rpc/Protocol.h"
#include "nixd/rpc/Transport.h"

#include <nixt/InitEval.h>

#include <unistd.h>

namespace {

using namespace nixd::rpc;

#define READ 0
#define WRITE 1

struct TestEvalProvider : testing::Test {
  int Pipes[2][2];

  TestEvalProvider() { nixt::initEval(); }
};

TEST_F(TestEvalProvider, ExprValue) {
  ASSERT_EQ(pipe(Pipes[READ]), 0);
  ASSERT_EQ(pipe(Pipes[WRITE]), 0);

  nixd::EvalProvider Provider(Pipes[READ][READ], Pipes[WRITE][WRITE]);

  sendPacket<Message<ExprValueParams>>(Pipes[READ][WRITE],
                                       {RPCKind::ExprValue, {}});

  close(Pipes[READ][WRITE]);

  [[maybe_unused]] int Result = Provider.run();

  auto Packet = recvPacket<ExprValueResponse>(Pipes[WRITE][READ]);

  ASSERT_EQ(Packet.ValueID, 0);
  ASSERT_EQ(Packet.ValueKind, 0);
}

} // namespace
