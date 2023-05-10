#include "lspserver/LSPServer.h"
#include "lspserver/Connection.h"
#include <llvm/Support/Error.h>

namespace lspserver {

void LSPServer::run() { In->loop(*this); }

bool LSPServer::onNotify(llvm::StringRef Method, llvm::json::Value Params) {
  log("<-- {0}", Method);
  if (Method == "exit")
    return false;
  auto Handler = Registry.NotificationHandlers.find(Method);
  if (Handler != Registry.NotificationHandlers.end()) {
    Handler->second(std::move(Params));
  } else {
    log("unhandled notification {0}", Method);
  }
  return true;
}

bool LSPServer::onCall(llvm::StringRef Method, llvm::json::Value Params,
                       llvm::json::Value ID) {
  log("<-- {0}({1})", Method, ID);
  auto Handler = Registry.MethodHandlers.find(Method);
  if (Handler != Registry.MethodHandlers.end())
    Handler->second(
        std::move(Params), [&](llvm::Expected<llvm::json::Value> Response) {
          if (Response) {
            log("--> reply:{0}({1}), response: {2} ", Method, ID, *Response);
            Out->reply(std::move(ID), std::move(Response));
          }
        });
  else
    return false;
  return true;
}

} // namespace lspserver
