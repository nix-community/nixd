#pragma once

#include "lspserver/Connection.h"
#include "lspserver/DraftStore.h"
#include "lspserver/Function.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"
#include "nixd/EvalDraftStore.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/JSON.h>

namespace nixd {

namespace configuration {

struct InstallableConfigurationItem {
  std::vector<std::string> args;
  std::string installable;
};

struct TopLevel {
  /// Nix installables that will be used for root translation unit.
  std::optional<InstallableConfigurationItem> installable;
};

bool fromJSON(const llvm::json::Value &Params, TopLevel &R, llvm::json::Path P);
bool fromJSON(const llvm::json::Value &Params, InstallableConfigurationItem &R,
              llvm::json::Path P);
} // namespace configuration

/// The server instance, nix-related language features goes here
class Server : public lspserver::LSPServer {

  EvalDraftStore DraftMgr;
  boost::asio::thread_pool Pool;

  lspserver::ClientCapabilities ClientCaps;

  configuration::TopLevel Config;

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

  llvm::unique_function<void(const lspserver::ConfigurationParams &,
                             lspserver::Callback<configuration::TopLevel>)>
      WorkspaceConfiguration;

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {
    Registry.addMethod("initialize", this, &Server::onInitialize);
    Registry.addMethod("textDocument/hover", this, &Server::onHover);
    Registry.addNotification("initialized", this, &Server::onInitialized);

    // Text Document Synchronization
    Registry.addNotification("textDocument/didOpen", this,
                             &Server::onDocumentDidOpen);
    Registry.addNotification("textDocument/didChange", this,
                             &Server::onDocumentDidChange);

    // Workspace
    Registry.addNotification("workspace/didChangeConfiguration", this,
                             &Server::onWorkspaceDidChangeConfiguration);
    PublishDiagnostic = mkOutNotifiction<lspserver::PublishDiagnosticsParams>(
        "textDocument/publishDiagnostics");
    WorkspaceConfiguration =
        mkOutMethod<lspserver::ConfigurationParams, configuration::TopLevel>(
            "workspace/configuration");
  }

  void fetchConfig();

  void onInitialize(const lspserver::InitializeParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onInitialized(const lspserver::InitializedParams &Params) {
    fetchConfig();
  }

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void publishStandaloneDiagnostic(lspserver::URIForFile Uri,
                                   std::string Content,
                                   std::optional<int64_t> LSPVersion);

  void onWorkspaceDidChangeConfiguration(
      const lspserver::DidChangeConfigurationParams &) {
    fetchConfig();
  }
  void onHover(const lspserver::TextDocumentPositionParams &,
               lspserver::Callback<llvm::json::Value>);
};

}; // namespace nixd
