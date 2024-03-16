#pragma once

#include "EvalClient.h"
#include "NixTU.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"

#include <boost/asio/thread_pool.hpp>

namespace nixd {

class Controller : public lspserver::LSPServer {
  std::unique_ptr<OwnedEvalClient> Eval;
  lspserver::DraftStore Store;

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;

  std::mutex TUsLock;
  llvm::StringMap<NixTU> TUs;

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

  void publishDiagnostics(lspserver::PathRef File,
                          std::optional<int64_t> Version,
                          const std::vector<nixf::Diagnostic> &Diagnostics);

public:
  Controller(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out);

  ~Controller() { Pool.join(); }

  void setLitTest(bool LitTest) { this->LitTest = LitTest; }
};

} // namespace nixd
