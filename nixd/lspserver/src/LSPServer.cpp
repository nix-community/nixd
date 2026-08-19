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

void LSPServer::run() {
  In->loop(*this);
  failPendingCalls("LSP input ended");
}

void LSPServer::failPendingCalls(std::string Reason) {
  std::map<int, Callback<llvm::json::Value>> Pending;
  std::string Failure;
  {
    std::lock_guard Guard(PendingCallsLock);
    if (CallsAccepted) {
      CallsAccepted = false;
      TerminalReason = std::move(Reason);
    }
    Failure = TerminalReason;
    if (SendsInFlight == 0)
      Pending.swap(PendingCalls);
  }
  for (auto &[ID, Reply] : Pending)
    Reply(error("{0}: request {1}", Failure, ID));
}

void LSPServer::callMethod(llvm::StringRef Method, llvm::json::Value Params,
                           Callback<llvm::json::Value> CB, OutboundPort *O) {
  std::optional<std::pair<int, Callback<llvm::json::Value>>> Evicted;
  std::optional<Callback<llvm::json::Value>> Rejected;
  std::string RejectionReason;
  int ID = 0;
  {
    std::lock_guard Guard(PendingCallsLock);
    if (!CallsAccepted) {
      Rejected = std::move(CB);
      RejectionReason = TerminalReason;
    } else {
      ID = TopID++;
      PendingCalls.emplace(ID, std::move(CB));
      if (PendingCalls.size() > MaxPendingCalls) {
        auto Begin = PendingCalls.begin();
        Evicted.emplace(Begin->first, std::move(Begin->second));
        PendingCalls.erase(Begin);
      }
      ++SendsInFlight;
    }
  }

  if (Rejected) {
    (*Rejected)(error("{0}: request rejected", RejectionReason));
    return;
  }

  log("--> call {0}({1})", Method, ID);
  std::error_code SendFailure = O->call(Method, std::move(Params), ID);

  std::map<int, Callback<llvm::json::Value>> Terminal;
  std::optional<Callback<llvm::json::Value>> FailedSend;
  std::string Failure;
  {
    std::lock_guard Guard(PendingCallsLock);
    assert(SendsInFlight > 0);
    --SendsInFlight;
    if (SendFailure) {
      auto It = PendingCalls.find(ID);
      if (It != PendingCalls.end()) {
        FailedSend = std::move(It->second);
        PendingCalls.erase(It);
      }
    }
    if (!CallsAccepted && SendsInFlight == 0) {
      Failure = TerminalReason;
      Terminal.swap(PendingCalls);
    }
  }

  if (FailedSend)
    (*FailedSend)(
        error("failed to send request {0}: {1}", ID, SendFailure.message()));
  for (auto &[PendingID, Reply] : Terminal)
    Reply(error("{0}: request {1}", Failure, PendingID));
  if (Evicted) {
    auto [EvictedID, Reply] = std::move(*Evicted);
    Reply(
        error("failed to receive a client reply for request ({0})", EvictedID));
    elog("more than {0} outstanding LSP calls, forgetting about {1}",
         MaxPendingCalls, EvictedID);
  }
}

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
                        log("--> reply:{0}({1})", Method, ID);
                        Out->reply(std::move(ID), std::move(Response));
                      } else {
                        llvm::Error Err = Response.takeError();
                        log("--> reply:{0}({1}) {2:ms}, error: {3}", Method, ID,
                            Err);
                        Out->reply(std::move(ID), std::move(Err));
                      }
                    });
  else
    return false;
  return true;
}

bool LSPServer::onReply(llvm::json::Value ID,
                        llvm::Expected<llvm::json::Value> Result) {
  log("<-- reply({0})", ID);
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

} // namespace lspserver
