#pragma once

#include "lspserver/Connection.h"
#include "lspserver/LSPBinder.h"
#include <llvm/Support/JSON.h>
#include <memory>

namespace lspserver {

class LSPServer : public MessageHandler {
private:
  std::unique_ptr<InboundPort> In;
  std::unique_ptr<OutboundPort> Out;

  bool onNotify(llvm::StringRef Method, llvm::json::Value) override;
  bool onCall(llvm::StringRef Method, llvm::json::Value Params,
              llvm::json::Value ID) override;
  bool onReply(llvm::json::Value ID,
               llvm::Expected<llvm::json::Value> Result) override {
    return true;
  }

protected:
  HandlerRegistry Registry;

public:
  LSPServer(std::unique_ptr<InboundPort> In, std::unique_ptr<OutboundPort> Out)
      : In(std::move(In)), Out(std::move(Out)){};
  void run();
};

} // namespace lspserver
