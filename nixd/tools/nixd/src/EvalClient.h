#pragma once

#include "nixd/rpc/Protocol.h"
#include "nixd/rpc/Transport.h"
#include "nixd/util/PipedProc.h"

#include <memory>

namespace nixd {

class EvalClient : public rpc::Transport {
  // Owned process of the evaluator
  std::unique_ptr<util::PipedProc> Proc;
  void handleInbound(const std::vector<char> &Buf) override{};

public:
  EvalClient(int InboundFD, int OutboundFD,
             std::unique_ptr<util::PipedProc> Proc)
      : rpc::Transport(InboundFD, OutboundFD), Proc(std::move(Proc)) {}

  virtual ~EvalClient() = default;

  /// Lanch nix-node-eval, with properly handled file descriptors.
  /// System-wide errno will be written into "Fail" variable and thus cannot be
  /// discarded.
  static std::unique_ptr<EvalClient> create(int &Fail);

  void registerBC(const rpc::RegisterBCParams &Params);

  rpc::ExprValueResponse exprValue(const rpc::ExprValueParams &Params);

  util::PipedProc *proc() { return Proc.get(); }
};

} // namespace nixd
