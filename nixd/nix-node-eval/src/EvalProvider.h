#pragma once

#include "nixd/rpc/Protocol.h"
#include "nixd/rpc/Transport.h"

#include <nixt/HookExpr.h>
#include <nixt/PtrPool.h>

#include <nix/nixexpr.hh>

namespace nixd {

class EvalProvider : public rpc::Transport {

  nixt::PtrPool<nix::Expr> Pool;
  nixt::ValueMap VMap;
  nixt::EnvMap EMap;
  std::unique_ptr<nix::EvalState> State;

  void handleInbound(const std::vector<char> &Buf) override;

public:
  EvalProvider(int InboundFD, int OutboundFD);

  void onRegisterBC(const rpc::RegisterBCParams &Params);

  rpc::ExprValueResponse onExprValue(const rpc::ExprValueParams &Params);
};

} // namespace nixd
