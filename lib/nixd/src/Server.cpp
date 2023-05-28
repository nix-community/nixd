#include "nixd/Server.h"
#include "lspserver/URI.h"
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

void Server::clearDiagnostic(lspserver::PathRef Path) {
  lspserver::URIForFile Uri = lspserver::URIForFile::canonicalize(Path, Path);
  clearDiagnostic(Uri);
}

void Server::clearDiagnostic(const lspserver::URIForFile &FileUri) {
  lspserver::PublishDiagnosticsParams Notification;
  Notification.uri = FileUri;
  Notification.diagnostics = {};
  PublishDiagnostic(Notification);
}

void Server::diagNixError(lspserver::PathRef Path, const nix::BaseError &NixErr,
                          std::optional<int64_t> Version) {
  using namespace lspserver;
  auto DiagVec = mkDiagnostics(NixErr);
  URIForFile Uri = URIForFile::canonicalize(Path, Path);
  PublishDiagnosticsParams Notification;
  Notification.uri = Uri;
  Notification.diagnostics = DiagVec;
  Notification.version = Version;
  PublishDiagnostic(Notification);
}

void Server::invalidateEvalCache() {
  {
    std::lock_guard<std::mutex> Guard(ResultLock);
    CachedResult = nullptr;
  }
}

void Server::withEval(
    std::string Fallback,
    llvm::unique_function<void(std::shared_ptr<EvalResult> Result)> Then) {
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
        // Workaround for display version
        for (auto ActiveFile : DraftMgr.getActiveFiles())
          clearDiagnostic(ActiveFile);

        // Publish all injection errors.
        for (const auto &[NixErr, ErrInfo] : Arg.InjectionErrors)
          diagNixError(ErrInfo.ActiveFile, *NixErr,
                       EvalDraftStore::decodeVersion(ErrInfo.Version));

        // TODO: extract real eval error source file
        clearDiagnostic("/");
        // And evaluation error
        try {
          if (Arg.EvalError)
            std::rethrow_exception(Arg.EvalError);
        } catch (nix::BaseError &Err) {
          diagNixError("/", Err, 0);
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
          return;
        }

        Then(nullptr);
      });
}

void Server::addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                         llvm::StringRef Version) {
  using namespace lspserver;

  // Since this file is updated, we first clear its diagnostic
  PublishDiagnosticsParams Notification;
  Notification.uri = URIForFile::canonicalize(File, File);
  Notification.diagnostics = {};
  Notification.version = DraftStore::decodeVersion(Version);
  PublishDiagnostic(Notification);

  DraftMgr.addDraft(File, Version, Contents);

  invalidateEvalCache();

  withEval(File.str(), [](std::shared_ptr<EvalResult> Result) {});
}

CompletionHelper::Items
CompletionHelper::fromStaticEnv(const nix::SymbolTable &STable,
                                const nix::StaticEnv &SEnv) {
  Items Result;
  for (auto [Symbol, Displ] : SEnv.vars) {
    std::string Name = STable[Symbol];
    if (Name.starts_with("__"))
      continue;
    Result.emplace_back(lspserver::CompletionItem{.label = Name});
  }
  return Result;
}

CompletionHelper::Items
CompletionHelper::fromEnvWith(const nix::SymbolTable &STable,
                              const nix::Env &NixEnv) {
  Items Result;
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    nix::Bindings::iterator BindingIt = NixEnv.values[0]->attrs->begin();
    while (BindingIt != NixEnv.values[0]->attrs->end()) {
      std::string Name = STable[BindingIt->name];
      Result.emplace_back(lspserver::CompletionItem{.label = Name});
      ++BindingIt;
    }
  }
  return Result;
}

CompletionHelper::Items
CompletionHelper::fromEnvRecursive(const nix::SymbolTable &STable,
                                   const nix::StaticEnv &SEnv,
                                   const nix::Env &NixEnv) {

  Items Result;
  if ((SEnv.up != nullptr) && (NixEnv.up != nullptr)) {
    Items Inherited = fromEnvRecursive(STable, *SEnv.up, *NixEnv.up);
    Result.insert(Result.end(), Inherited.begin(), Inherited.end());
  }

  auto StaticEnvItems = fromStaticEnv(STable, SEnv);
  Result.insert(Result.end(), StaticEnvItems.begin(), StaticEnvItems.end());

  auto EnvItems = fromEnvWith(STable, NixEnv);
  Result.insert(Result.end(), EnvItems.begin(), EnvItems.end());

  return Result;
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
      {"completionProvider", {}}};

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
            invalidateEvalCache();
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
    Registry.addMethod("textDocument/completion", this, &Server::onCompletion);
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
      HoverFile, [HoverFile, Paras, Reply = std::move(Reply)](
                     std::shared_ptr<EvalResult> Result) mutable {
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
          Reply(Hover{{
                          MarkupKind::Markdown,
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

void Server::onCompletion(const lspserver::CompletionParams &Params,
                          lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  std::string CompletionFile = Params.textDocument.uri.file().str();

  withEval(CompletionFile,
           [=, Reply = std::move(Reply),
            this](std::shared_ptr<EvalDraftStore::EvalResult> Result) mutable {
             if (Result == nullptr && LastValidResult == nullptr) {
               Reply(CompletionList{});
               return;
             }
             if (Result == nullptr)
               Result = LastValidResult;
             auto State = Result->State;
             lspserver::CompletionList List;
             // Lookup an AST node, and get it's 'Env' after evaluation
             CompletionHelper::Items Items;
             try {
               auto AST = Result->EvalASTForest.at(CompletionFile);
               auto *Node = AST->lookupPosition(Params.position);

               auto ExprEnv = AST->getEnv(Node);

               Items = CompletionHelper::fromEnvRecursive(
                   State->symbols, *State->staticBaseEnv, *ExprEnv);

             } catch (const std::out_of_range &) {
               // Fallback to staticEnv only
               Items = CompletionHelper::fromStaticEnv(State->symbols,
                                                       *State->staticBaseEnv);
             }
             List.items.insert(List.items.end(), Items.begin(), Items.end());
             Reply(List);
           });
}

} // namespace nixd
