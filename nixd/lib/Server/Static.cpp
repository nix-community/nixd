#include "nixd/Server/Server.h"

namespace nixd {

void Server::switchToStatic() {
  initWorker();
  Role = ServerRole::Static;
  EvalDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");

  Registry.addMethod("nixd/ipc/textDocument/documentLink", this,
                     &Server::onStaticDocumentLink);
  Registry.addMethod("nixd/ipc/textDocument/documentSymbol", this,
                     &Server::onStaticDocumentSymbol);

  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &Server::onStaticCompletion);

  Registry.addMethod("nixd/ipc/textDocument/definition", this,
                     &Server::onStaticDefinition);

  Registry.addMethod("nixd/ipc/textDocument/rename", this,
                     &Server::onStaticRename);

  evalInstallable(Config.getEvalDepth());
  mkOutNotifiction<ipc::WorkerMessage>("nixd/ipc/finished")(
      ipc::WorkerMessage{WorkspaceVersion});
}

void Server::onStaticDocumentSymbol(
    const lspserver::TextDocumentIdentifier &Params,
    lspserver::Callback<std::vector<lspserver::DocumentSymbol>> Reply) {
  using Symbols = std::vector<lspserver::DocumentSymbol>;
  using namespace lspserver;
  withAST<Symbols>(Params.uri.file().str(),
                   ReplyRAII<Symbols>(std::move(Reply)),
                   [Params, File = Params.uri.file().str(),
                    State = IER->Session->getState()](
                       const nix::ref<EvalAST> &AST, ReplyRAII<Symbols> &&RR) {
                     struct VTy : RecursiveASTVisitor<VTy> {
                       decltype(AST) &VAST;
                       nix::EvalState &State;

                       std::set<const nix::Expr *> Visited;

                       // Per-node symbols should be collected here
                       Symbols CurrentSymbols;

                       bool traverseExprVar(const nix::ExprVar *E) {
                         try {
                           DocumentSymbol S;
                           S.name = State.symbols[E->name];
                           S.kind = SymbolKind::Variable;
                           S.selectionRange = VAST->nPair(VAST->getPos(E));
                           S.range = S.selectionRange;
                           CurrentSymbols.emplace_back(std::move(S));
                         } catch (...) {
                         }
                         return true;
                       }

                       bool traverseExprLambda(const nix::ExprLambda *E) {
                         if (!E->hasFormals())
                           return true;
                         for (const auto &Formal : E->formals->formals) {
                           DocumentSymbol S;
                           S.name = State.symbols[Formal.name];
                           S.kind = SymbolKind::Module;
                           S.selectionRange = VAST->nPair(Formal.pos);
                           S.range = S.selectionRange;
                           CurrentSymbols.emplace_back(std::move(S));
                         }
                         RecursiveASTVisitor<VTy>::traverseExprLambda(E);
                         return true;
                       }

                       bool traverseExprAttrs(const nix::ExprAttrs *E) {
                         Symbols AttrSymbols = CurrentSymbols;
                         CurrentSymbols = {};
                         for (const auto &[Symbol, Def] : E->attrs) {
                           DocumentSymbol S;
                           S.name = State.symbols[Symbol];
                           traverseExpr(Def.e);
                           S.children = std::move(CurrentSymbols);
                           S.kind = SymbolKind::Field;
                           try {
                             S.range = VAST->nPair(VAST->getPos(Def.e));
                           } catch (std::out_of_range &) {
                             S.range = VAST->nPair(Def.pos);
                           }
                           S.selectionRange = VAST->nPair(Def.pos);
                           S.range = S.range / S.selectionRange;
                           AttrSymbols.emplace_back(std::move(S));
                         }
                         CurrentSymbols = std::move(AttrSymbols);
                         return true;
                       }

                     } V{.VAST = AST, .State = *State};
                     V.traverseExpr(AST->root());
                     RR.Response = V.CurrentSymbols;
                   });
}

void Server::onStaticDocumentLink(
    const lspserver::TextDocumentIdentifier &Params,
    lspserver::Callback<std::vector<lspserver::DocumentLink>> Reply) {
  using Links = std::vector<lspserver::DocumentLink>;
  using namespace lspserver;
  withAST<Links>(
      Params.uri.file().str(), ReplyRAII<Links>(std::move(Reply)),
      [Params, File = Params.uri.file().str()](const nix::ref<EvalAST> &AST,
                                               ReplyRAII<Links> &&RR) {
        struct LinkFinder : RecursiveASTVisitor<LinkFinder> {
          decltype(AST) &LinkAST;
          const std::string &File;
          Links Result;
          bool visitExprPath(const nix::ExprPath *EP) {
            auto Range = LinkAST->lRange(EP);
            if (Range.has_value()) {
              try {
                std::string Path =
                    nix::resolveExprPath(CanonPath(EP->s)).to_string();

                Result.emplace_back(DocumentLink{
                    .range = Range.value(),
                    .target = URIForFile::canonicalize(Path, Path)});
              } catch (...) {
              }
            }
            return true;
          }
        } Finder{.LinkAST = AST, .File = File};
        Finder.traverseExpr(AST->root());
        RR.Response = Finder.Result;
      });
}

void Server::onStaticRename(
    const lspserver::RenameParams &Params,
    lspserver::Callback<std::vector<lspserver::TextEdit>> Reply) {
  using namespace lspserver;
  using TextEdits = std::vector<lspserver::TextEdit>;
  auto Action = [&Params](const nix::ref<EvalAST> &AST,
                          ReplyRAII<TextEdits> &&RR) {
    RR.Response = error("no suitable renaming action available");
    if (const auto *EVar = dynamic_cast<const nix::ExprVar *>(
            AST->lookupContainMin(Params.position))) {
      RR.Response = AST->rename(EVar, Params.newName);
      return;
    }
    if (auto Def = AST->lookupDef(Params.position)) {
      RR.Response = AST->rename(*Def, Params.newName);
      return;
    }
  };
  withAST<TextEdits>(Params.textDocument.uri.file().str(),
                     ReplyRAII<TextEdits>(std::move(Reply)), std::move(Action));
}

void Server::onStaticCompletion(const lspserver::CompletionParams &Params,
                                lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  auto Action = [Params, this](const nix::ref<EvalAST> &AST,
                               ReplyRAII<CompletionList> &&RR) {
    auto State = IER->Session->getState();
    // Lookup an AST node, and get it's 'Env' after evaluation
    CompletionHelper::Items Items;
    lspserver::log("current trigger character is {0}",
                   Params.context.triggerCharacter);

    if (Params.context.triggerCharacter == ".") {
      RR.Response =
          error("statically lookup attr fields is not yet implemented");
      return;
    }

    const auto *Node = AST->lookupContainMin(Params.position);

    std::vector<Symbol> Symbols;
    AST->collectSymbols(Node, Symbols);
    // Insert symbols to our completion list.
    std::transform(Symbols.begin(), Symbols.end(), std::back_inserter(Items),
                   [&](const Symbol &V) -> decltype(Items)::value_type {
                     decltype(Items)::value_type R;
                     R.kind = CompletionItemKind::Interface;
                     R.label = State->symbols[V];
                     return R;
                   });

    CompletionHelper::fromStaticEnv(State->symbols, *State->staticBaseEnv,
                                    Items);
    // Make the response.
    CompletionList List;
    List.isIncomplete = false;
    List.items = Items;
    RR.Response = List;
  };

  withAST<CompletionList>(Params.textDocument.uri.file().str(),
                          ReplyRAII<CompletionList>(std::move(Reply)),
                          std::move(Action));
}

void Server::onStaticDefinition(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<lspserver::Location> Reply) {
  using namespace lspserver;
  withAST<Location>(
      Params.textDocument.uri.file().str(),
      ReplyRAII<Location>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Location> &&RR) {
        auto State = IER->Session->getState();

        const auto *Node = AST->lookupContainMin(Params.position);
        if (!Node)
          return;

        // try to find the location binds to the variable.
        if (const auto *EVar = dynamic_cast<const nix::ExprVar *>(Node)) {
          try {
            RR.Response = Location{Params.textDocument.uri,
                                   AST->defRange(AST->def(EVar))};
          } catch (std::out_of_range &) {
            RR.Response = error("location not available");
          }
          return;
        }
        RR.Response = error("requested expression is not an ExprVar.");
        return;
      });
}

} // namespace nixd
