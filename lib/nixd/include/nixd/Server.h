#pragma once

#include "lspserver/Connection.h"
#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/JSON.h>

namespace nixd {
/// The server instance, nix-related language features goes here
class Server : public lspserver::LSPServer {

  lspserver::DraftStore DraftMgr;

  std::shared_ptr<const std::string> getDraft(lspserver::PathRef File) const;

  void addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                   llvm::StringRef Version) {
    DraftMgr.addDraft(File, Version, Contents);
  }

  void removeDocument(lspserver::PathRef File) { DraftMgr.removeDraft(File); }

  /// LSP defines file versions as numbers that increase.
  /// treats them as opaque and therefore uses strings instead.
  static std::string encodeVersion(std::optional<int64_t> LSPVersion);

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {
    Registry.addMethod("initialize", this, &Server::onInitialize);
    Registry.addNotification("initialized", this, &Server::onInitialized);

    // Text Document Synchronization
    Registry.addNotification("textDocument/didOpen", this,
                             &Server::onDocumentDidOpen);
    Registry.addNotification("textDocument/didChange", this,
                             &Server::onDocumentDidChange);
    PublishDiagnostic = mkOutNotifiction<lspserver::PublishDiagnosticsParams>(
        "textDocument/publishDiagnostics");
  }

  void onInitialize(const lspserver::InitializeParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onInitialized(const lspserver::InitializedParams &Params){};

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void publishStandaloneDiagnostic(lspserver::URIForFile Uri,
                                   std::string Content,
                                   std::optional<int64_t> LSPVersion);
};

}; // namespace nixd
