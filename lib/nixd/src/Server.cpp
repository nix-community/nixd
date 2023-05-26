#include "nixd/Server.h"
#include "lspserver/Logger.h"
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
#include <sstream>
#include <string>
#include <variant>
namespace fs = std::filesystem;
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
      {"hoverProvider", true},
  };

  llvm::json::Object Result{
      {{"serverInfo",
        llvm::json::Object{{"name", "nixd"}, {"version", "0.0.0"}}},
       {"capabilities", std::move(ServerCaps)}}};
  Reply(std::move(Result));
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
    auto *E = NixState->parseExprFromString(std::move(Content),
                                            Path.remove_filename());
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

void Server::onHover(const lspserver::TextDocumentPositionParams &Paras,
                     lspserver::Callback<llvm::json::Value> Reply) {
  std::string HoverFile = Paras.textDocument.uri.file().str();
  DraftMgr.withEvaluation(
      Pool, {"--file", HoverFile}, "",
      [=, Reply = std::move(Reply)](
          std::variant<const std::exception *,
                       nix::ref<EvalDraftStore::EvaluationResult>>
              EvalResult) mutable {
        nix::ref<EvalDraftStore::EvaluationResult> Result =
            std::get<1>(EvalResult);
        try {
          std::get<1>(EvalResult); // w contains int, not float: will throw
        } catch (const std::bad_variant_access &) {
          std::exception *EvalExcept = std::get<0>(EvalResult);
          Reply(nullptr);
          PublishDiagnostic(lspserver::PublishDiagnosticsParams{
              .uri = Paras.textDocument.uri,
              .diagnostics =
                  mkDiagnostics(*dynamic_cast<const nix::Error *>(EvalExcept)),
              .version = 1});
          return;
        }
        auto Forest = Result->EvalASTForest;
        auto AST = Forest.at(HoverFile);
        auto *Node = AST->lookupPosition(Paras.position);
        std::stringstream NodeOut;
        Node->show(Result->State->symbols, NodeOut);
        nix::Value Value{};
        try {
          Value = AST->getValue(Node);
        } catch (const std::out_of_range &ValueNotFound) {
          Reply(lspserver::Hover{
              .contents = {.value = "Value Not Found"},
          });
          return;
        }
        std::stringstream Res{};
        Value.print(Result->State->symbols, Res);
        Reply(lspserver::Hover{
            .contents =
                {
                    .value = Res.str(),
                },
        });
      });
}

} // namespace nixd
