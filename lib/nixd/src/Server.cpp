#include "nixd/Server.h"
#include "nixd/Diagnostic.h"
#include "nixd/EvalDraftStore.h"
#include "nixd/Expr.h"

#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/ScopedPrinter.h>

#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/store-api.hh>
#include <nix/util.hh>

#include <exception>
#include <filesystem>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

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

std::tuple<nix::Strings, std::string>
TopLevel::getInstallable(std::string Fallback) const {
  if (auto Installable = installable) {
    auto ConfigArgs = Installable->args;
    lspserver::log("using client specified installable: [{0}] {1}",
                   llvm::iterator_range(ConfigArgs.begin(), ConfigArgs.end()),
                   Installable->installable);
    return std::tuple{nix::Strings(ConfigArgs.begin(), ConfigArgs.end()),
                      Installable->installable};
  }
  // Fallback to current file otherwise.
  return std::tuple{nix::Strings{"--file", std::move(Fallback)},
                    std::string{""}};
}

} // namespace configuration

void Server::withEval(std::string Fallback,
                      llvm::unique_function<void(
                          std::shared_ptr<EvalDraftStore::EvalResult> Result)>
                          Then) {
  using namespace lspserver;

  {
    std::lock_guard<std::mutex> Guard(ResultLock);
    if (CachedResult) {
      Then(CachedResult);
      return;
    }
  }

  const auto &[CommandLine, Installable] =
      Config.getInstallable(std::move(Fallback));
  DraftMgr.withEvaluation(
      Pool, CommandLine, Installable,
      [this,
       Then = std::move(Then)](const EvalDraftStore::CallbackArg &Arg) mutable {
        // Publish all injection errors.
        for (const auto &[NixErr, ErrInfo] : Arg.InjectionErrors) {
          auto DiagVec = mkDiagnostics(*NixErr);
          URIForFile Uri =
              URIForFile::canonicalize(ErrInfo.ActiveFile, ErrInfo.ActiveFile);
          PublishDiagnosticsParams Notification;
          Notification.uri = Uri;
          Notification.diagnostics = DiagVec;
          Notification.version = DraftStore::decodeVersion(ErrInfo.Version);
          PublishDiagnostic(Notification);
        }

        // And evaluation error
        try {
          if (Arg.EvalError)
            std::rethrow_exception(Arg.EvalError);
        } catch (nix::BaseError &Err) {
          // We do not know which file caused this error at all.
          auto DiagVec = mkDiagnostics(Err);
          PublishDiagnosticsParams Notification;
          Notification.diagnostics = DiagVec;
          PublishDiagnostic(Notification);
        } catch (std::exception &Except) {
          elog("unhandled exception: {0}", Except.what());
        } catch (...) {
          elog("unhandled exception (unknown)");
        }

        if (Arg.Result) {
          std::lock_guard<std::mutex> Guard(ResultLock);
          LastValidResult = Arg.Result;
          CachedResult = Arg.Result;
          Then(Arg.Result);
        }
      });
}

void Server::addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                         llvm::StringRef Version) {
  using namespace lspserver;

  // Since this file is update, we first clear its diagnostic
  PublishDiagnosticsParams Notification;
  Notification.uri = URIForFile::canonicalize(File, File);
  Notification.diagnostics = {};
  Notification.version = DraftStore::decodeVersion(Version);
  PublishDiagnostic(Notification);

  DraftMgr.addDraft(File, Version, Contents);

  {
    std::lock_guard<std::mutex> Guard(ResultLock);
    CachedResult = nullptr;
  }

  withEval(File.str(),
           [](std::shared_ptr<EvalDraftStore::EvalResult> Result) {});
}

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
      {"hoverProvider", true},
  };

  llvm::json::Object Result{
      {{"serverInfo",
        llvm::json::Object{{"name", "nixd"}, {"version", "0.0.0"}}},
       {"capabilities", std::move(ServerCaps)}}};
  Reply(std::move(Result));
}

void Server::fetchConfig() {
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

Server::Server(std::unique_ptr<lspserver::InboundPort> In,
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

void Server::onDocumentDidOpen(
    const lspserver::DidOpenTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();

  const std::string &Contents = Params.textDocument.text;

  addDocument(File, Contents, encodeVersion(Params.textDocument.version));
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

void Server::onHover(const lspserver::TextDocumentPositionParams &Paras,
                     lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  std::string HoverFile = Paras.textDocument.uri.file().str();
  withEval(
      HoverFile,
      [HoverFile, Paras, Reply = std::move(Reply)](
          std::shared_ptr<EvalDraftStore::EvalResult> Result) mutable {
        if (Result == nullptr) {
          Reply(Hover{{}, std::nullopt});
          return;
        }
        auto Forest = Result->EvalASTForest;
        try {
          auto AST = Forest.at(HoverFile);
          auto *Node = AST->lookupPosition(Paras.position);
          const auto *ExprName = getExprName(Node);
          std::string HoverText;
          try {
            auto Value = AST->getValue(Node);
            std::stringstream Res{};
            Value.print(Result->State->symbols, Res);
            HoverText =
                llvm::formatv("## {0} \n Value: `{1}`", ExprName, Res.str());
          } catch (const std::out_of_range &) {
            // No such value, just reply dummy item
            std::stringstream NodeOut;
            Node->show(Result->State->symbols, NodeOut);
            lspserver::vlog("no associated value on node {0}!", NodeOut.str());
            HoverText = llvm::formatv("`{0}`", ExprName);
          }
          Reply(lspserver::Hover{{
                                     lspserver::MarkupKind::Markdown,
                                     HoverText,
                                 },
                                 std::nullopt});
        } catch (const std::out_of_range &) {
          // Probably out of range in Forest.at
          // Ignore this expression, and reply dummy value.
          Reply(Hover{{}, std::nullopt});
        }
      });
}
} // namespace nixd
