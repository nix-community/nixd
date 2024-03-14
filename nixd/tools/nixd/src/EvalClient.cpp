#include "nixd-config.h"

#include "EvalClient.h"

#include "nixd/rpc/Protocol.h"
#include "nixd/util/ForkPiped.h"

#include <bc/Read.h>
#include <bc/Write.h>

#include <unistd.h>

namespace nixd {

using namespace rpc;
using namespace nixd::util;

template <class T> using M = Message<T>;

std::unique_ptr<EvalClient> EvalClient::create(int &Fail) {
  int In;
  int Out;
  int Err;

  pid_t Child = forkPiped(In, Out, Err);
  if (Child == 0) {
    execl(NIXD_LIBEXEC "/nix-node-eval", "nix-node-eval", nullptr);
    exit(-1);
  } else if (Child < 0) {
    // Error.
    Fail = Child;
    return nullptr;
  }

  Fail = 0;
  // Parent process.
  auto Proc = std::make_unique<PipedProc>(Child, In, Out, Err);
  return std::make_unique<EvalClient>(Out, In, std::move(Proc));
}

void EvalClient::registerBC(const RegisterBCParams &Params) {
  sendPacket<M<RegisterBCParams>>({RPCKind::RegisterBC, Params});
}

ExprValueResponse EvalClient::exprValue(const ExprValueParams &Params) {
  sendPacket<M<ExprValueParams>>({RPCKind::ExprValue, Params});
  return recvPacket<ExprValueResponse>();
}

} // namespace nixd
