#include "lspserver/LSPServer.h"
#include "lspserver/Connection.h"
#include "lspserver/Function.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <mutex>

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
    Handler->second(std::move(Params),
                    [=, Method = std::string(Method),
                     this](llvm::Expected<llvm::json::Value> Response) mutable {
                      if (Response) {
                        log("--> reply:{0}({1}), response: {2} ", Method, ID,
                            *Response);
                        Out->reply(std::move(ID), std::move(Response));
                      }
                    });
  else
    return false;
  return true;
}

bool LSPServer::onReply(llvm::json::Value ID,
                        llvm::Expected<llvm::json::Value> Result) {
  std::lock_guard<std::mutex> Guard(PendingCallsLock);
  log("<-- reply({0})", ID);
  if (auto I = ID.getAsInteger()) {
    if (PendingCalls.contains(*I)) {
      PendingCalls[*I](std::move(Result));
      PendingCalls.erase(*I);
      return true;
    }
    elog("received a reply with ID {0}, but there was no such call", ID);
    // Ignore this error
    return true;
  }
  elog("Cannot retrieve message ID from json: {0}", ID);
  return false;
}

int LSPServer::bindReply(Callback<llvm::json::Value> CB) {
  std::lock_guard<std::mutex> _(PendingCallsLock);
  int Ret = TopID++;
  PendingCalls[Ret] = std::move(CB);

  // Check the limit
  if (PendingCalls.size() > MaxPendingCalls) {
    auto Begin = PendingCalls.begin();
    auto [ID, OldestCallback] =
        std::tuple{Begin->first, std::move(Begin->second)};
    OldestCallback(
        error("failed to receive a client reply for request ({0})", ID));
    elog("more than {0} outstanding LSP calls, forgetting about {1}",
         MaxPendingCalls, ID);
    PendingCalls.erase(Begin);
  }
  return Ret;
}

} // namespace lspserver
