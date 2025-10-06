#include "lspserver/LSPServer.h"
#include "lspserver/Connection.h"
#include "lspserver/Function.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/Compiler.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <mutex>
#include <stdexcept>

namespace lspserver {

void LSPServer::run() { In->loop(*this); }

bool LSPServer::onNotify(llvm::StringRef Method, llvm::json::Value Params) {
  maybeLog("<-- {0}", Method);
  if (Method == "exit")
    return false;
  auto Handler = Registry.NotificationHandlers.find(Method);
  if (Handler != Registry.NotificationHandlers.end()) {
    Handler->second(std::move(Params));
  } else {
    maybeLog("unhandled notification {0}", Method);
  }
  return true;
}

bool LSPServer::onCall(llvm::StringRef Method, llvm::json::Value Params,
                       llvm::json::Value ID) {
  maybeLog("<-- {0}({1})", Method, ID);
  auto Handler = Registry.MethodHandlers.find(Method);
  if (Handler != Registry.MethodHandlers.end())
    Handler->second(std::move(Params),
                    [=, Method = std::string(Method),
                     this](llvm::Expected<llvm::json::Value> Response) mutable {
                      if (Response) {
                        maybeLog("--> reply:{0}({1})", Method, ID);
                        Out->reply(std::move(ID), std::move(Response));
                      } else {
                        llvm::Error Err = Response.takeError();
                        maybeLog("--> reply:{0}({1}) {2:ms}, error: {3}",
                                 Method, ID, Err);
                        Out->reply(std::move(ID), std::move(Err));
                      }
                    });
  else
    return false;
  return true;
}

bool LSPServer::onReply(llvm::json::Value ID,
                        llvm::Expected<llvm::json::Value> Result) {
  maybeLog("<-- reply({0})", ID);
  std::optional<Callback<llvm::json::Value>> CB;

  if (auto OptI = ID.getAsInteger()) {
    if (LLVM_UNLIKELY(*OptI > INT_MAX))
      throw std::logic_error("jsonrpc: id is too large (> INT_MAX)");
    std::lock_guard<std::mutex> Guard(PendingCallsLock);
    auto I = static_cast<int>(*OptI);
    if (PendingCalls.contains(I)) {
      CB = std::move(PendingCalls[I]);
      PendingCalls.erase(I);
    }
  } else {
    throw std::logic_error("jsonrpc: not an integer message ID");
  }
  if (LLVM_UNLIKELY(!CB)) {
    elog("received a reply with ID {0}, but there was no such call", ID);
    // Ignore this error
    return true;
  }
  // Invoke the callback outside of the critical zone, because we just do not
  // need to lock PendingCalls.
  (*CB)(std::move(Result));
  return true;
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
