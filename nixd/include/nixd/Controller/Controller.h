#pragma once

#include "Configuration.h"
#include "EvalClient.h"
#include "NixTU.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Protocol.h"
#include "nixd/Eval/AttrSetClient.h"

#include <boost/asio/thread_pool.hpp>

namespace nixd {

class Controller : public lspserver::LSPServer {
public:
  using OptionMapTy = std::map<std::string, std::unique_ptr<AttrSetClientProc>>;

private:
  std::unique_ptr<OwnedEvalClient> Eval;

  // Use this worker for evaluating nixpkgs.
  std::unique_ptr<AttrSetClientProc> NixpkgsEval;

  std::mutex OptionsLock;
  // Map of option providers.
  //
  // e.g. "nixos" -> nixos worker
  //      "home-manager" -> home-manager worker
  OptionMapTy Options; // GUARDED_BY(OptionsLock)

  AttrSetClientProc &nixpkgsEval() {
    assert(NixpkgsEval);
    return *NixpkgsEval;
  }

  AttrSetClient *nixpkgsClient() { return nixpkgsEval().client(); }

  void evalExprWithProgress(AttrSetClient &Client, const EvalExprParams &Params,
                            std::string_view Description);

  lspserver::DraftStore Store;

  lspserver::ClientCapabilities ClientCaps;

  std::mutex ConfigLock;
  Configuration Config; // GUARDED_BY(ConfigLock)

  llvm::unique_function<void(const lspserver::ConfigurationParams &,
                             lspserver::Callback<llvm::json::Value>)>
      WorkspaceConfiguration;

  void workspaceConfiguration(const lspserver::ConfigurationParams &Params,
                              lspserver::Callback<llvm::json::Value> Reply);

  /// \brief Update the configuration, do necessary adjusting for updates.
  ///
  /// \example If asked to change eval settings, send eval requests to workers.
  void updateConfig(Configuration NewConfig);

  /// \brief Get configuration from LSP client. Update the config.
  void fetchConfig();

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;
  llvm::unique_function<void(const lspserver::WorkDoneProgressCreateParams &,
                             lspserver::Callback<std::nullptr_t>)>
      CreateWorkDoneProgress;

  void
  createWorkDoneProgress(const lspserver::WorkDoneProgressCreateParams &Params);

  llvm::unique_function<void(
      const lspserver::ProgressParams<lspserver::WorkDoneProgressBegin> &)>
      BeginWorkDoneProgress;

  void beginWorkDoneProgress(
      const lspserver::ProgressParams<lspserver::WorkDoneProgressBegin>
          &Params) {
    if (ClientCaps.WorkDoneProgress)
      BeginWorkDoneProgress(Params);
  }

  llvm::unique_function<void(
      const lspserver::ProgressParams<lspserver::WorkDoneProgressReport> &)>
      ReportWorkDoneProgress;

  void reportWorkDoneProgress(
      const lspserver::ProgressParams<lspserver::WorkDoneProgressReport>
          &Params) {
    if (ClientCaps.WorkDoneProgress)
      ReportWorkDoneProgress(Params);
  }

  llvm::unique_function<void(
      lspserver::ProgressParams<lspserver::WorkDoneProgressEnd>)>
      EndWorkDoneProgress;

  void endWorkDoneProgress(
      const lspserver::ProgressParams<lspserver::WorkDoneProgressEnd> &Params) {
    if (ClientCaps.WorkDoneProgress)
      EndWorkDoneProgress(Params);
  }

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

  /// Action right after a document is added (including updates).
  void actOnDocumentAdd(lspserver::PathRef File,
                        std::optional<int64_t> Version);

  void removeDocument(lspserver::PathRef File);

  void onInitialize(const lspserver::InitializeParams &Params,
                    lspserver::Callback<llvm::json::Value> Reply);

  void onInitialized(const lspserver::InitializedParams &Params);

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
                    lspserver::Callback<llvm::json::Value> Reply);

  void
  onReferences(const lspserver::TextDocumentPositionParams &Params,
               lspserver::Callback<std::vector<lspserver::Location>> Reply);

  void onDocumentHighlight(
      const lspserver::TextDocumentPositionParams &Params,
      lspserver::Callback<std::vector<lspserver::DocumentHighlight>> Reply);

  void onDocumentLink(
      const lspserver::DocumentLinkParams &Params,
      lspserver::Callback<std::vector<lspserver::DocumentLink>> Reply);

  void publishDiagnostics(lspserver::PathRef File,
                          std::optional<int64_t> Version,
                          const std::vector<nixf::Diagnostic> &Diagnostics);

  void onRename(const lspserver::RenameParams &Params,
                lspserver::Callback<lspserver::WorkspaceEdit> Reply);

  void onPrepareRename(const lspserver::TextDocumentPositionParams &Params,
                       lspserver::Callback<lspserver::Range> Reply);

  void onFormat(const lspserver::DocumentFormattingParams &Params,
                lspserver::Callback<std::vector<lspserver::TextEdit>> Reply);

  //---------------------------------------------------------------------------/
  // Workspace features
  //---------------------------------------------------------------------------/
  void onDidChangeConfiguration(
      const lspserver::DidChangeConfigurationParams &Params);

public:
  Controller(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out);

  ~Controller() override { Pool.join(); }

  bool isReadyToEval() { return Eval && Eval->ready(); }
};

} // namespace nixd
