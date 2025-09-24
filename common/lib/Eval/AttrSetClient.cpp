#include "nixd-config.h"

#include "nixd/Eval/AttrSetClient.h"

#include <signal.h> // NOLINT(modernize-deprecated-headers)

using namespace nixd;
using namespace lspserver;

AttrSetClient::AttrSetClient(std::unique_ptr<lspserver::InboundPort> In,
                             std::unique_ptr<lspserver::OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)) {
  EvalExpr = mkOutMethod<EvalExprParams, EvalExprResponse>(rpcMethod::EvalExpr);
  AttrPathInfo = mkOutMethod<AttrPathInfoParams, AttrPathInfoResponse>(
      rpcMethod::AttrPathInfo);
  AttrPathComplete =
      mkOutMethod<AttrPathCompleteParams, AttrPathCompleteResponse>(
          rpcMethod::AttrPathComplete);
  OptionInfo = mkOutMethod<AttrPathInfoParams, OptionInfoResponse>(
      rpcMethod::OptionInfo);
  OptionComplete = mkOutMethod<AttrPathCompleteParams, OptionCompleteResponse>(
      rpcMethod::OptionComplete);
  Exit = mkOutNotifiction<std::nullptr_t>(rpcMethod::Exit);
}

const char *AttrSetClient::getExe() {
  if (const char *Env = std::getenv("NIXD_ATTRSET_EVAL"))
    return Env;
  return NIXD_LIBEXEC "/nixd-attrset-eval";
}

AttrSetClientProc::AttrSetClientProc(const std::function<int()> &Action)
    : Proc(Action), Client(Proc.mkIn(), Proc.mkOut()),
      Input([this]() { Client.run(); }) {}

AttrSetClient *AttrSetClientProc::client() {
  if (!kill(Proc.proc().PID, 0))
    return &Client;
  return nullptr;
}
