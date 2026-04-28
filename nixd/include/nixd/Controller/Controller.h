#pragma once

#include "Configuration.h"
#include "EvalClient.h"
#include "NixTU.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Protocol.h"
#include "nixd/Eval/AttrSetClient.h"
#include "nixf/Basic/Diagnostic.h"

#include <boost/asio/thread_pool.hpp>

#include <set>

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

  /// Request the client to show a document (file or URL).
  /// @since LSP 3.16.0
  llvm::unique_function<void(
      const lspserver::ShowDocumentParams &,
      lspserver::Callback<lspserver::ShowDocumentResult>)>
      ShowDocument;

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

  mutable std::mutex TUsLock;
  llvm::StringMap<std::shared_ptr<NixTU>> TUs;

  std::shared_ptr<const NixTU> getTU(std::string_view File) const {
    using lspserver::error;
    std::lock_guard G(TUsLock);
    if (!TUs.count(File)) [[unlikely]] {
      lspserver::elog("cannot get translation unit: {0}", File);
      return nullptr;
    }
    return TUs.lookup(File);
  }

  static std::shared_ptr<nixf::Node> getAST(const NixTU &TU) {
    using lspserver::error;
    if (!TU.ast()) {
      lspserver::elog("AST is null on this unit");
      return nullptr;
    }
    return TU.ast();
  }

  std::shared_ptr<const nixf::Node> getAST(std::string_view File) const {
    auto TU = getTU(File);
    return TU ? getAST(*TU) : nullptr;
  }

#if BOOST_VERSION < 108800
  // Default constructor is broken in Boost 1.87, fixed in 1.88:
  // https://github.com/boostorg/asio/commit/30b5974ed34bfa321d268b3135ffaffcb261461a
  boost::asio::thread_pool Pool{
      static_cast<size_t>(boost::asio::detail::default_thread_pool_size())};
#else
  boost::asio::thread_pool Pool{};
#endif

  /// Action right after a document is added (including updates).
  void actOnDocumentAdd(lspserver::PathRef File,
                        std::optional<int64_t> Version);

  void removeDocument(lspserver::PathRef File);

  void onInitialize(const lspserver::InitializeParams &Params,
                    lspserver::Callback<llvm::json::Value> Reply);

  void onInitialized(const lspserver::InitializedParams &Params);

  bool ReceivedShutdown = false;

  void onShutdown(const lspserver::NoParams &,
                  lspserver::Callback<std::nullptr_t> Reply);

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void onDocumentDidClose(const lspserver::DidCloseTextDocumentParams &Params);

  void
  onCodeAction(const lspserver::CodeActionParams &Params,
               lspserver::Callback<std::vector<lspserver::CodeAction>> Reply);

  void onCodeActionResolve(const lspserver::CodeAction &Params,
                           lspserver::Callback<lspserver::CodeAction> Reply);

  void onHover(const lspserver::TextDocumentPositionParams &Params,
               lspserver::Callback<std::optional<lspserver::Hover>> Reply);

  void onDocumentSymbol(
      const lspserver::DocumentSymbolParams &Params,
      lspserver::Callback<std::vector<lspserver::DocumentSymbol>> Reply);

  void onFoldingRange(
      const lspserver::FoldingRangeParams &Params,
      lspserver::Callback<std::vector<lspserver::FoldingRange>> Reply);

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

  std::set<nixf::Diagnostic::DiagnosticKind>
      SuppressedDiagnostics; // GUARDED_BY(SuppressedDiagnosticsLock)

  std::mutex SuppressedDiagnosticsLock;

  /// Update the suppressing set. There might be some invalid names, should be
  /// logged then.
  void updateSuppressed(const std::vector<std::string> &Sup);

  /// Determine whether or not this diagnostic is suppressed.
  bool isSuppressed(nixf::Diagnostic::DiagnosticKind Kind);
  void publishDiagnostics(lspserver::PathRef File,
                          std::optional<int64_t> Version, std::string_view Src,
                          const std::vector<nixf::Diagnostic> &Diagnostics);

  void onRename(const lspserver::RenameParams &Params,
                lspserver::Callback<lspserver::WorkspaceEdit> Reply);

  void
  onPrepareRename(const lspserver::TextDocumentPositionParams &Params,
                  lspserver::Callback<std::optional<lspserver::Range>> Reply);

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
