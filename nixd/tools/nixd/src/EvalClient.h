#pragma once

#include "nixd/rpc/Protocol.h"

#include "nixd/util/PipedProc.h"

#include <lspserver/LSPServer.h>

#include <memory>
#include <thread>

namespace nixd {

class EvalClient : public lspserver::LSPServer {
public:
  llvm::unique_function<void(const rpc::RegisterBCParams &)> RegisterBC;
  llvm::unique_function<void(const rpc::ExprValueParams &,
                             lspserver::Callback<rpc::ExprValueResponse>)>
      ExprValue;

  EvalClient(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out)
      : lspserver::LSPServer(std::move(In), std::move(Out)) {
    RegisterBC = mkOutNotifiction<rpc::RegisterBCParams>("registerBC");
    ExprValue =
        mkOutMethod<rpc::ExprValueParams, rpc::ExprValueResponse>("exprValue");
  }

  virtual ~EvalClient() = default;
};

class OwnedEvalClient : public EvalClient {
  std::unique_ptr<util::PipedProc> Proc;
  std::unique_ptr<llvm::raw_fd_ostream> Stream;

  std::thread Input;

public:
  OwnedEvalClient(std::unique_ptr<lspserver::InboundPort> In,
                  std::unique_ptr<lspserver::OutboundPort> Out,
                  std::unique_ptr<util::PipedProc> Proc,
                  std::unique_ptr<llvm::raw_fd_ostream> Stream)
      : EvalClient(std::move(In), std::move(Out)), Proc(std::move(Proc)),
        Stream(std::move(Stream)) {

    Input = std::thread([this]() { run(); });
  }

  util::PipedProc &proc() { return *Proc; }

  ~OwnedEvalClient() {
    closeInbound();
    Input.join();
  }

  /// Lanch nix-node-eval, with properly handled file descriptors.
  /// System-wide errno will be written into "Fail" variable and thus cannot be
  /// discarded.
  static std::unique_ptr<OwnedEvalClient> create(int &Fail);
};

} // namespace nixd
