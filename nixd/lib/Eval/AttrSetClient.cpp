#include "nixd-config.h"

#include "nixd/Eval/AttrSetClient.h"

#include <signal.h> // NOLINT(modernize-deprecated-headers)

using namespace nixd;
using namespace lspserver;

AttrSetClient::AttrSetClient(std::unique_ptr<lspserver::InboundPort> In,
                             std::unique_ptr<lspserver::OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)) {
  EvalExpr = mkOutMethod<EvalExprParams, EvalExprResponse>("attrset/evalExpr");
  AttrPathInfo = mkOutMethod<AttrPathInfoParams, AttrPathInfoResponse>(
      "attrset/attrpathInfo");
  AttrPathComplete =
      mkOutMethod<AttrPathCompleteParams, AttrPathCompleteResponse>(
          "attrset/attrpathComplete");
  OptionInfo =
      mkOutMethod<AttrPathInfoParams, OptionInfoResponse>("attrset/optionInfo");
  OptionComplete = mkOutMethod<AttrPathCompleteParams, OptionCompleteResponse>(
      "attrset/optionComplete");
  Exit = mkOutNotifiction<std::nullptr_t>("exit");
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
  if (!kill(Proc.Proc->PID, 0))
    return &Client;
  return nullptr;
}
