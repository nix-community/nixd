#pragma once

#include "EvalClient.h"
#include "NixTU.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Protocol.h"
#include "nixd/Eval/AttrSetClient.h"

#include <boost/asio/thread_pool.hpp>

namespace nixd {

class Controller : public lspserver::LSPServer {
  std::unique_ptr<OwnedEvalClient> Eval;

  // Use this worker for evaluating nixpkgs.
  std::unique_ptr<AttrSetClientProc> NixpkgsEval;

  AttrSetClientProc &nixpkgsEval() {
    assert(NixpkgsEval);
    return *NixpkgsEval;
  }

  AttrSetClient *nixpkgsClient() { return nixpkgsEval().client(); }

  lspserver::DraftStore Store;

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;

  std::mutex TUsLock;
  llvm::StringMap<std::shared_ptr<NixTU>> TUs;

  template <class T>
  std::shared_ptr<NixTU> getTU(std::string File, lspserver::Callback<T> &Reply,
                               bool Ignore = false) {
    using lspserver::error;
    std::lock_guard G(TUsLock);
    if (!TUs.count(File)) [[unlikely]] {
      if (!Ignore) {
        Reply(error("cannot find corresponding AST on file {0}", File));
        return nullptr;
      }
      Reply(T{}); // Reply a default constructed response.
      lspserver::elog("cannot get translation unit: {0}", File);
      return nullptr;
    }
    return TUs[File];
  }

  template <class T>
  std::shared_ptr<nixf::Node> getAST(const NixTU &TU,
                                     lspserver::Callback<T> &Reply) {
    using lspserver::error;
    if (!TU.ast()) {
      Reply(error("AST is null on this unit"));
      return nullptr;
    }
    return TU.ast();
  }

  boost::asio::thread_pool Pool;

  /// In lit-test mode. Disable some concurrency for better text-testing.
  bool LitTest;

  /// Action right after a document is added (including updates).
  void actOnDocumentAdd(lspserver::PathRef File,
                        std::optional<int64_t> Version);

  void removeDocument(lspserver::PathRef File) { Store.removeDraft(File); }

  void onInitialize( // NOLINT(readability-convert-member-functions-to-static)
      [[maybe_unused]] const lspserver::InitializeParams &Params,
      lspserver::Callback<llvm::json::Value> Reply);

  void
  onInitialized([[maybe_unused]] const lspserver::InitializedParams &Params) {}

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void onDocumentDidClose(const lspserver::DidCloseTextDocumentParams &Params);

  void
  onCodeAction(const lspserver::CodeActionParams &Params,
               lspserver::Callback<std::vector<lspserver::CodeAction>> Reply);

  void onHover(const lspserver::TextDocumentPositionParams &Params,
               lspserver::Callback<std::optional<lspserver::Hover>> Reply);

  void onDocumentSymbol(
      const lspserver::DocumentSymbolParams &Params,
      lspserver::Callback<std::vector<lspserver::DocumentSymbol>> Reply);

  void onSemanticTokens(const lspserver::SemanticTokensParams &Params,
                        lspserver::Callback<lspserver::SemanticTokens> Reply);

  void
  onInlayHint(const lspserver::InlayHintsParams &Params,
              lspserver::Callback<std::vector<lspserver::InlayHint>> Reply);

  void onCompletion(const lspserver::CompletionParams &Params,
                    lspserver::Callback<lspserver::CompletionList> Reply);

  void
  onCompletionItemResolve(const lspserver::CompletionItem &Params,
                          lspserver::Callback<lspserver::CompletionItem> Reply);

  void onDefinition(const lspserver::TextDocumentPositionParams &Params,
                    lspserver::Callback<lspserver::Location> Reply);

  void
  onReferences(const lspserver::TextDocumentPositionParams &Params,
               lspserver::Callback<std::vector<lspserver::Location>> Reply);

  void onDocumentHighlight(
      const lspserver::TextDocumentPositionParams &Params,
      lspserver::Callback<std::vector<lspserver::DocumentHighlight>> Reply);

  void publishDiagnostics(lspserver::PathRef File,
                          std::optional<int64_t> Version,
                          const std::vector<nixf::Diagnostic> &Diagnostics);

  void onRename(const lspserver::RenameParams &Params,
                lspserver::Callback<lspserver::WorkspaceEdit> Reply);

  void onPrepareRename(const lspserver::TextDocumentPositionParams &Params,
                       lspserver::Callback<lspserver::Range> Reply);

public:
  Controller(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out);

  ~Controller() override { Pool.join(); }

  void setLitTest(bool LitTest) { this->LitTest = LitTest; }

  bool isReadyToEval() { return Eval && Eval->ready(); }
};

} // namespace nixd
