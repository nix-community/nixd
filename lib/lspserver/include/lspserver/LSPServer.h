#pragma once

#include "lspserver/Connection.h"
#include "lspserver/Function.h"
#include "lspserver/LSPBinder.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <memory>

namespace lspserver {

/// LSPServer wraps inputs & outputs, associate message IDs between calls/reply,
/// and provide type-safe interfaces.
class LSPServer : public MessageHandler {
private:
  std::unique_ptr<InboundPort> In;
  std::unique_ptr<OutboundPort> Out;

  bool onNotify(llvm::StringRef Method, llvm::json::Value) override;
  bool onCall(llvm::StringRef Method, llvm::json::Value Params,
              llvm::json::Value ID) override;
  bool onReply(llvm::json::Value ID,
               llvm::Expected<llvm::json::Value> Result) override;

  std::mutex PendingCallsLock;

  /// Calls to the client sent, and waiting for the response.
  /// The callback function will be invoked when we get the result.
  ///
  /// If the call has no response for a long time, it should be removed and
  /// associated an error.
  std::map<int, Callback<llvm::json::Value>> PendingCalls;

  /// Number of maximum callbacks stored in the structure.
  /// Give an error to the oldest callback (least ID) while exceeding this
  /// value.
  static constexpr int MaxPendingCalls = 100;

  int TopID;

  /// Allocate an "ID" (as returned value) for this callback.
  int bindReply(Callback<llvm::json::Value>);

  void callMethod(llvm::StringRef Method, llvm::json::Value Params,
                  Callback<llvm::json::Value> CB) {
    llvm::json::Value ID{bindReply(std::move(CB))};
    Out->call(Method, Params, ID);
  }

protected:
  HandlerRegistry Registry;

  /// \return a unique_function that could be invoked to notify the client (i.e.
  /// no replies are expected).
  template <class T>
  llvm::unique_function<void(const T &)>
  mkOutNotifiction(llvm::StringRef Method) {
    return [=](const T &Params) { Out->notify(Method, Params); };
  }

  /// \return a unique_function that could be invoked to "call" the client (i.e.
  /// clients are expected to reply our requests).
  template <class ParamTy, class ResponseTy>
  llvm::unique_function<void(const ParamTy &, Callback<ResponseTy>)>
  mkOutMethod(llvm::StringRef Method) {
    return [&](const ParamTy &Params, Callback<ResponseTy> Reply) {
      callMethod(Method, Params,
                 [=, Reply = std::move(Reply)](
                     llvm::Expected<llvm::json::Value> Response) {
                   if (!Response)
                     return Reply(Response.takeError());
                   Reply(parseParam<ResponseTy>(std::move(*Response), Method,
                                                "reply"));
                 });
    };
  }

public:
  LSPServer(std::unique_ptr<InboundPort> In, std::unique_ptr<OutboundPort> Out)
      : In(std::move(In)), Out(std::move(Out)){};
  void run();
};

} // namespace lspserver
