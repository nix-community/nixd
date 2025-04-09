#pragma once

#include "nixd/Protocol/Protocol.h"

#include <lspserver/Function.h>
#include <lspserver/LSPServer.h>

#include <nixt/HookExpr.h>
#include <nixt/PtrPool.h>

#include <nix/expr/nixexpr.hh>

namespace nixd {

class EvalProvider : public lspserver::LSPServer {

  nixt::PtrPool<nix::Expr> Pool;
  nixt::ValueMap VMap;
  nixt::EnvMap EMap;
  std::unique_ptr<nix::EvalState> State;

  llvm::unique_function<void(int)> Ready;

public:
  EvalProvider(std::unique_ptr<lspserver::InboundPort> In,
               std::unique_ptr<lspserver::OutboundPort> Out);

  void onRegisterBC(const rpc::RegisterBCParams &Params);

  void onExprValue(const rpc::ExprValueParams &Params,
                   lspserver::Callback<rpc::ExprValueResponse>);
};

} // namespace nixd
