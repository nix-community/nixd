#include "nixd/Diagnostic.h"
#include "nixd/Server.h"

#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/globals.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/util.hh>

namespace nixd {

CompletionHelper::Items
CompletionHelper::fromStaticEnv(const nix::SymbolTable &STable,
                                const nix::StaticEnv &SEnv) {
  Items Result;
  for (auto [Symbol, Displ] : SEnv.vars) {
    std::string Name = STable[Symbol];
    if (Name.starts_with("__"))
      continue;

    // Variables in static envs, let's mark it as "Constant".
    Result.emplace_back(lspserver::CompletionItem{
        .label = Name, .kind = lspserver::CompletionItemKind::Constant});
  }
  return Result;
}

CompletionHelper::Items
CompletionHelper::fromEnvWith(const nix::SymbolTable &STable,
                              const nix::Env &NixEnv) {
  Items Result;
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    for (const auto &SomeAttr : *NixEnv.values[0]->attrs) {
      std::string Name = STable[SomeAttr.name];
      Result.emplace_back(lspserver::CompletionItem{
          .label = Name, .kind = lspserver::CompletionItemKind::Variable});
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

void Server::switchToEvaluator() {
  assert(Role == ServerRole::Controller &&
         "Must switch from controller's fork!");
  Role = ServerRole::Evaluator;

  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
  nix::initGC();

  WorkerDiagnostic =
      mkOutNotifiction<ipc::Diagnostics>("nixd/worker/diagnostic");

  eval("");
}

lspserver::PublishDiagnosticsParams
Server::diagNixError(lspserver::PathRef Path, const nix::BaseError &NixErr,
                     std::optional<int64_t> Version) {
  using namespace lspserver;
  auto DiagVec = mkDiagnostics(NixErr);
  URIForFile Uri = URIForFile::canonicalize(Path, Path);
  PublishDiagnosticsParams Notification;
  Notification.uri = Uri;
  Notification.diagnostics = DiagVec;
  Notification.version = Version;
  return Notification;
}

void Server::eval(const std::string &Fallback) {
  assert(Role != ServerRole::Controller &&
         "eval must be called in child workers.");
  auto Session = std::make_unique<IValueEvalSession>();

  auto [Args, Installable] = Config.getInstallable(Fallback);

  Session->parseArgs(Args);

  auto ILR = DraftMgr.injectFiles(Session->getState());

  ipc::Diagnostics Diagnostics;
  for (const auto &[ErrObject, ErrInfo] : ILR.InjectionErrors) {
    Diagnostics.Params.emplace_back(
        diagNixError(ErrInfo.ActiveFile, *ErrObject,
                     decltype(DraftMgr)::decodeVersion(ErrInfo.Version)));
  }
  Diagnostics.WorkspaceVersion = WorkspaceVersion;

  WorkerDiagnostic(Diagnostics);

  if (!ILR.InjectionErrors.empty()) {
    if (!ILR.Forest.empty())
      FCache.NonEmptyResult = {std::move(ILR.Forest), std::move(Session)};
  } else {
    const std::string EvalationErrorFile = "/evaluation-error-file.nix";
    try {
      Session->eval(Installable);
      FCache.EvaluatedResult = {std::move(ILR.Forest), std::move(Session)};
    } catch (nix::BaseError &BE) {
    } catch (...) {
    }
  }
}

void Server::onCompletion(const lspserver::CompletionParams &Params,
                          lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  std::string CompletionFile = Params.textDocument.uri.file().str();

  struct ReplyRAII {
    decltype(Reply) R;
    CompletionList Response;
    ReplyRAII(decltype(R) R) : R(std::move(R)) {}
    ~ReplyRAII() { R(Response); };
  } RR(std::move(Reply));

  const auto *IER =
      FCache.searchAST(CompletionFile, ForestCache::ASTPreference::Evaluated);

  if (!IER)
    return;

  auto AST = IER->Forest.at(CompletionFile);

  auto State = IER->Session->getState();
  // Lookup an AST node, and get it's 'Env' after evaluation
  CompletionHelper::Items Items;
  lspserver::vlog("current trigger character is {0}",
                  Params.context.triggerCharacter);

  if (Params.context.triggerCharacter == ".") {
    try {
      auto *Node = AST->lookupPosition(Params.position);
      auto Value = AST->getValue(Node);
      if (Value.type() == nix::ValueType::nAttrs) {
        // Traverse attribute bindings
        for (auto Binding : *Value.attrs) {
          Items.emplace_back(
              CompletionItem{.label = State->symbols[Binding.name],
                             .kind = CompletionItemKind::Field});
        }
      }
    } catch (...) {
    }
  } else {
    try {
      auto *Node = AST->lookupPosition(Params.position);
      const auto *ExprEnv = AST->getEnv(Node);

      Items = CompletionHelper::fromEnvRecursive(
          State->symbols, *State->staticBaseEnv, *ExprEnv);
    } catch (const std::out_of_range &) {
      // Fallback to staticEnv only
      Items = CompletionHelper::fromStaticEnv(State->symbols,
                                              *State->staticBaseEnv);
    }
  }
  RR.Response.isIncomplete = false;
  RR.Response.items = Items;
}

void Server::onHover(const lspserver::TextDocumentPositionParams &Paras,
                     lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  std::string HoverFile = Paras.textDocument.uri.file().str();
  const auto *IER =
      FCache.searchAST(HoverFile, ForestCache::ASTPreference::NonEmpty);

  if (!IER)
    return;

  auto AST = IER->Forest.at(HoverFile);

  struct ReplyRAII {
    decltype(Reply) R;
    Hover Response;
    ReplyRAII(decltype(R) R) : R(std::move(R)) {}
    ~ReplyRAII() { R(Response); };
  } RR(std::move(Reply));

  auto *Node = AST->lookupPosition(Paras.position);
  const auto *ExprName = getExprName(Node);
  std::string HoverText;
  RR.Response.contents.kind = MarkupKind::Markdown;
  try {
    auto Value = AST->getValue(Node);
    std::stringstream Res{};
    Value.print(IER->Session->getState()->symbols, Res);
    HoverText = llvm::formatv("## {0} \n Value: `{1}`", ExprName, Res.str());
  } catch (const std::out_of_range &) {
    // No such value, just reply dummy item
    std::stringstream NodeOut;
    Node->show(IER->Session->getState()->symbols, NodeOut);
    lspserver::vlog("no associated value on node {0}!", NodeOut.str());
    HoverText = llvm::formatv("`{0}`", ExprName);
  }
  RR.Response.contents.value = HoverText;
}

} // namespace nixd
