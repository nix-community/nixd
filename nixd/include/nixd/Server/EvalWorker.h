#pragma once

#include "nixd/Server/EvalDraftStore.h"
#include "nixd/Support/Diagnostic.h"
#include "nixd/Support/JSONSerialization.h"
#include "nixd/Support/ReplyRAII.h"

#include "lspserver/LSPServer.h"

namespace nixd {

class EvalWorker : public lspserver::LSPServer {
  llvm::unique_function<void(const ipc::Diagnostics &)> EvalDiagnostic;

  std::unique_ptr<IValueEvalResult> IER;

  template <class ReplyTy>
  void withAST(
      const std::string &, ReplyRAII<ReplyTy>,
      llvm::unique_function<void(nix::ref<EvalAST>, ReplyRAII<ReplyTy> &&)>);

public:
  EvalWorker(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out);

  void onEvalDefinition(const lspserver::TextDocumentPositionParams &,
                        lspserver::Callback<lspserver::Location>);

  void onEvalHover(const lspserver::TextDocumentPositionParams &,
                   lspserver::Callback<llvm::json::Value>);

  void onEvalCompletion(const lspserver::CompletionParams &,
                        lspserver::Callback<llvm::json::Value>);
};

template <class ReplyTy>
void EvalWorker::withAST(
    const std::string &RequestedFile, ReplyRAII<ReplyTy> RR,
    llvm::unique_function<void(nix::ref<EvalAST>, ReplyRAII<ReplyTy> &&)>
        Action) {
  try {
    auto AST = IER->Forest.at(RequestedFile);
    try {
      Action(AST, std::move(RR));
    } catch (std::exception &E) {
      RR.Response =
          lspserver::error("something uncaught in the AST action, reason {0}",
                           stripANSI(E.what()));
    }
  } catch (std::out_of_range &E) {
    RR.Response = lspserver::error("no AST available on requested file {0}",
                                   RequestedFile);
  }
}

} // namespace nixd
