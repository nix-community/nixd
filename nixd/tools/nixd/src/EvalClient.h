#pragma once

#include "nixd/rpc/Protocol.h"

#include "nixd/util/PipedProc.h"

#include <lspserver/LSPServer.h>

#include <memory>
#include <thread>

namespace nixd {

class EvalClient : public lspserver::LSPServer {
  std::atomic<bool> Ready;

public:
  llvm::unique_function<void(const rpc::RegisterBCParams &,
                             lspserver::Callback<rpc::RegisterBCResponse>)>
      RegisterBC;
  llvm::unique_function<void(const rpc::ExprValueParams &,
                             lspserver::Callback<rpc::ExprValueResponse>)>
      ExprValue;

  EvalClient(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out);

  virtual ~EvalClient() = default;

  void onReady(const int &Flags) {
    lspserver::log(
        "nix-node-eval({0}) reported it's ready for processing requests",
        Flags);
    Ready = true;
  }

  bool ready() { return Ready; }
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
