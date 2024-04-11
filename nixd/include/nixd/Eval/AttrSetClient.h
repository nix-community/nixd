#pragma once

#include "nixd/Protocol/AttrSet.h"
#include "nixd/Support/StreamProc.h"

#include <lspserver/LSPServer.h>

#include <thread>

namespace nixd {

class AttrSetClient : public lspserver::LSPServer {

  llvm::unique_function<void(const EvalExprParams &Params,
                             lspserver::Callback<EvalExprResponse> Reply)>
      EvalExpr;

  llvm::unique_function<void(const AttrPathInfoParams &Params,
                             lspserver::Callback<AttrPathInfoResponse> Reply)>
      AttrPathInfo;

  llvm::unique_function<void(
      const AttrPathCompleteParams &Params,
      lspserver::Callback<AttrPathCompleteResponse> Reply)>
      AttrPathComplete;

public:
  AttrSetClient(std::unique_ptr<lspserver::InboundPort> In,
                std::unique_ptr<lspserver::OutboundPort> Out);

  /// \brief Request eval some expression.
  /// The expression should be evaluted to attrset.
  void evalExpr(const EvalExprParams &Params,
                lspserver::Callback<EvalExprResponse> Reply) {
    return EvalExpr(Params, std::move(Reply));
  }

  void attrpathInfo(const AttrPathInfoParams &Params,
                    lspserver::Callback<AttrPathInfoResponse> Reply) {
    AttrPathInfo(Params, std::move(Reply));
  }

  void attrpathComplete(const AttrPathCompleteParams &Params,
                        lspserver::Callback<AttrPathCompleteResponse> Reply) {
    AttrPathComplete(Params, std::move(Reply));
  }

  /// Get executable path for launching the server.
  /// \returns null terminated string.
  static const char *getExe();
};

class AttrSetClientProc {
  StreamProc Proc;
  AttrSetClient Client;
  std::thread Input;

public:
  /// \brief Check if the process is still alive
  /// \returns nullptr if it has been dead.
  AttrSetClient *client();
  ~AttrSetClientProc() {
    Client.closeInbound();
    Input.join();
  }

  /// \see StreamProc::StreamProc
  AttrSetClientProc(const std::function<int()> &Action);
};

} // namespace nixd
