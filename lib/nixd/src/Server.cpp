#include "nixd/Server.h"
#include "globals.hh"
#include "lspserver/Path.h"
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/ScopedPrinter.h>

#include <nix/canon-path.hh>
#include <nix/eval.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>

#include <exception>

namespace nixd {

void Server::onInitialize(const lspserver::InitializeParams &InitializeParams,
                          lspserver::Callback<llvm::json::Value> Reply) {
  llvm::json::Object ServerCaps{
      {"textDocumentSync",
       llvm::json::Object{
           {"openClose", true},
           {"change", (int)lspserver::TextDocumentSyncKind::Incremental},
           {"save", true},
       }},
      {"workspaceSymbolProvider", true},
  };

  llvm::json::Object Result{
      {{"serverInfo",
        llvm::json::Object{{"name", "nixd"}, {"version", "0.0.0"}}},
       {"capabilities", std::move(ServerCaps)}}};
  Reply(std::move(Result));

  nix::initNix();
  nix::initPlugins();
  nix::initGC();
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
  lspserver::elog("nix settings: {0}", nix::settings.storeUri.get());
  auto NixStore = nix::openStore();
  auto State = std::make_unique<nix::EvalState>(nix::Strings(), NixStore);
  try {
    State->parseExprFromFile(File.str());
  } catch (const nix::ParseError &PE) {
    lspserver::elog("got nix parse error! {0}", PE.msg());
    // TODO: provide diagnostic
  } catch (const nix::UndefinedVarError &UVE) {
    lspserver::elog("got nix undefined variable error! {0}", UVE.msg());
    // TODO: provide diagnostic
  } catch (const std::exception &E) {
    lspserver::elog("unknown error while parsing! {0}", E.what());
  }
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
}

} // namespace nixd
