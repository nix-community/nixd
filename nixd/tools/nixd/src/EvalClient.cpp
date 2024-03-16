#include "nixd-config.h"

#include "EvalClient.h"

#include "nixd/util/ForkPiped.h"

#include <bc/Read.h>
#include <bc/Write.h>

#include <llvm/Support/raw_ostream.h>

#include <unistd.h>

namespace nixd {

using namespace rpc;
using namespace nixd::util;

std::unique_ptr<OwnedEvalClient> OwnedEvalClient::create(int &Fail) {
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
  auto InP = std::make_unique<lspserver::InboundPort>(
      Out, lspserver::JSONStreamStyle::Standard);

  auto ProcFdStream = std::make_unique<llvm::raw_fd_ostream>(In, false);

  auto OutP = std::make_unique<lspserver::OutboundPort>(*ProcFdStream, false);
  return std::make_unique<OwnedEvalClient>(std::move(InP), std::move(OutP),
                                           std::move(Proc),
                                           std::move(ProcFdStream));
}

EvalClient::EvalClient(std::unique_ptr<lspserver::InboundPort> In,
                       std::unique_ptr<lspserver::OutboundPort> Out)
    : lspserver::LSPServer(std::move(In), std::move(Out)), Ready(false) {
  RegisterBC = mkOutNotifiction<rpc::RegisterBCParams>("registerBC");
  ExprValue =
      mkOutMethod<rpc::ExprValueParams, rpc::ExprValueResponse>("exprValue");
  Registry.addNotification("ready", this, &EvalClient::onReady);
}

} // namespace nixd
