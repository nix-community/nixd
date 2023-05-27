#include "nixd/Server.h"
#include "nixd/Diagnostic.h"

#include "lspserver/Path.h"
#include "lspserver/Protocol.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/ScopedPrinter.h>

#include <nix/eval.hh>
#include <nix/store-api.hh>

#include <filesystem>
namespace fs = std::filesystem;
namespace nixd {

namespace configuration {

bool fromJSON(const llvm::json::Value &Params, TopLevel &R,
              llvm::json::Path P) {
  const auto *PA = Params.getAsArray();
  auto X = PA->front();
  llvm::json::ObjectMapper O(X, P);
  return O && O.map("installable", R.installable);
}

bool fromJSON(const llvm::json::Value &Params, nix::Strings &R,
              llvm::json::Path P) {
  std::vector<std::string> RVec{std::begin(R), std::end(R)};
  return fromJSON(Params, RVec, P);
}

bool fromJSON(const llvm::json::Value &Params, InstallableConfigurationItem &R,
              llvm::json::Path P) {
  llvm::json::ObjectMapper O(Params, P);
  return O && O.map("args", R.args) && O.map("installable", R.installable);
}

} // namespace configuration

void Server::onInitialize(const lspserver::InitializeParams &InitializeParams,
                          lspserver::Callback<llvm::json::Value> Reply) {
  ClientCaps = InitializeParams.capabilities;
  llvm::json::Object ServerCaps{
      {"textDocumentSync",
       llvm::json::Object{
           {"openClose", true},
           {"change", (int)lspserver::TextDocumentSyncKind::Incremental},
           {"save", true},
       }},
  };

  llvm::json::Object Result{
      {{"serverInfo",
        llvm::json::Object{{"name", "nixd"}, {"version", "0.0.0"}}},
       {"capabilities", std::move(ServerCaps)}}};
  Reply(std::move(Result));
}

void Server::onInitialized(const lspserver::InitializedParams &Params) {
  if (ClientCaps.WorkspaceConfiguration) {
    WorkspaceConfiguration(
        lspserver::ConfigurationParams{
            std::vector<lspserver::ConfigurationItem>{
                lspserver::ConfigurationItem{.section = "nixd"}}},
        [this](llvm::Expected<configuration::TopLevel> Response) {
          if (Response) {
            Config = std::move(Response.get());
          }
        });
  }
}

void Server::onWorkspaceDidChangeConfiguration(
    const lspserver::DidChangeConfigurationParams &Params) {
  // fetch configuration again.
  if (ClientCaps.WorkspaceConfiguration) {
    WorkspaceConfiguration(
        lspserver::ConfigurationParams{
            std::vector<lspserver::ConfigurationItem>{
                lspserver::ConfigurationItem{.section = "nixd"}}},
        [this](llvm::Expected<configuration::TopLevel> Response) {
          if (Response) {
            Config = std::move(Response.get());
          }
        });
  }
}

std::string Server::encodeVersion(std::optional<int64_t> LSPVersion) {
  return LSPVersion ? llvm::to_string(*LSPVersion) : "";
}

std::shared_ptr<const std::string>
Server::getDraft(lspserver::PathRef File) const {
  auto Draft = DraftMgr.getDraft(File);
  if (!Draft)
    return nullptr;
  return std::move(Draft->Contents);
}

void Server::onDocumentDidOpen(
    const lspserver::DidOpenTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();

  const std::string &Contents = Params.textDocument.text;

  addDocument(File, Contents, encodeVersion(Params.textDocument.version));
  publishStandaloneDiagnostic(Params.textDocument.uri, Contents,
                              Params.textDocument.version);
}

void Server::onDocumentDidChange(
    const lspserver::DidChangeTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();
  auto Code = getDraft(File);
  if (!Code) {
    lspserver::log("Trying to incrementally change non-added document: {0}",
                   File);
    return;
  }
  std::string NewCode(*Code);
  for (const auto &Change : Params.contentChanges) {
    if (auto Err = applyChange(NewCode, Change)) {
      // If this fails, we are most likely going to be not in sync anymore
      // with the client.  It is better to remove the draft and let further
      // operations fail rather than giving wrong results.
      removeDocument(File);
      lspserver::elog("Failed to update {0}: {1}", File, std::move(Err));
      return;
    }
  }
  addDocument(File, NewCode, encodeVersion(Params.textDocument.version));
  publishStandaloneDiagnostic(Params.textDocument.uri, NewCode,
                              Params.textDocument.version);
}

void Server::publishStandaloneDiagnostic(lspserver::URIForFile Uri,
                                         std::string Content,
                                         std::optional<int64_t> LSPVersion) {
  auto NixStore = nix::openStore();
  auto NixState = std::make_unique<nix::EvalState>(nix::Strings{}, NixStore);
  try {
    fs::path Path = Uri.file().str();
    auto *E =
        NixState->parseExprFromString(std::move(Content), Path.remove_filename());
    nix::Value V;
    NixState->eval(E, V);
  } catch (const nix::Error &PE) {
    PublishDiagnostic(lspserver::PublishDiagnosticsParams{
        .uri = Uri, .diagnostics = mkDiagnostics(PE), .version = LSPVersion});
    return;
  } catch (...) {
    return;
  }
  PublishDiagnostic(lspserver::PublishDiagnosticsParams{
      .uri = Uri, .diagnostics = {}, .version = LSPVersion});
}

} // namespace nixd
