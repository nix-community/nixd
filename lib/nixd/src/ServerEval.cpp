#include "nixd/AST.h"
#include "nixd/Diagnostic.h"
#include "nixd/Expr.h"
#include "nixd/Position.h"
#include "nixd/Server.h"
#include "nixd/nix/Option.h"
#include "nixd/nix/Value.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>

#include <llvm/ADT/StringRef.h>

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nixd {

struct CompletionHelper {
  using Items = std::vector<lspserver::CompletionItem>;
  static void fromEnvRecursive(const nix::SymbolTable &STable,
                               const nix::StaticEnv &SEnv,
                               const nix::Env &NixEnv, Items &Items);
  static void fromEnvWith(const nix::SymbolTable &STable,
                          const nix::Env &NixEnv, Items &Items);
  static void fromStaticEnv(const nix::SymbolTable &STable,
                            const nix::StaticEnv &SEnv, Items &Items);
};

void CompletionHelper::fromStaticEnv(const nix::SymbolTable &STable,
                                     const nix::StaticEnv &SEnv, Items &Items) {
  for (auto [Symbol, Displ] : SEnv.vars) {
    std::string Name = STable[Symbol];
    if (Name.starts_with("__"))
      continue;

    // Variables in static envs, let's mark it as "Constant".
    Items.emplace_back(lspserver::CompletionItem{
        .label = Name, .kind = lspserver::CompletionItemKind::Constant});
  }
}

void CompletionHelper::fromEnvWith(const nix::SymbolTable &STable,
                                   const nix::Env &NixEnv, Items &Items) {
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    for (const auto &SomeAttr : *NixEnv.values[0]->attrs) {
      std::string Name = STable[SomeAttr.name];
      Items.emplace_back(lspserver::CompletionItem{
          .label = Name, .kind = lspserver::CompletionItemKind::Variable});
    }
  }
}

void CompletionHelper::fromEnvRecursive(const nix::SymbolTable &STable,
                                        const nix::StaticEnv &SEnv,
                                        const nix::Env &NixEnv, Items &Items) {

  if ((SEnv.up != nullptr) && (NixEnv.up != nullptr))
    fromEnvRecursive(STable, *SEnv.up, *NixEnv.up, Items);

  fromStaticEnv(STable, SEnv, Items);
  fromEnvWith(STable, NixEnv, Items);
}

void Server::switchToEvaluator(lspserver::PathRef File) {
  initWorker();
  Role = ServerRole::Evaluator;
  EvalDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");
  evalInstallable(File, Config.getEvalDepth());
  mkOutNotifiction<ipc::WorkerMessage>("nixd/ipc/eval/finished")(
      ipc::WorkerMessage{WorkspaceVersion});
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

void Server::evalInstallable(lspserver::PathRef File, int Depth = 0) {
  assert(Role != ServerRole::Controller && "must be called in child workers.");
  auto Session = std::make_unique<IValueEvalSession>();

  decltype(Config.eval->target) I;

  if (!Config.eval.has_value())
    I = std::nullopt;
  else
    I = Config.eval->target;

  auto ShoudlEval = I.has_value() && !I->empty();

  if (ShoudlEval)
    Session->parseArgs(I->ndArgs());

  auto ILR = DraftMgr.injectFiles(Session->getState());

  ipc::Diagnostics Diagnostics;
  for (const auto &[ErrObject, ErrInfo] : ILR.InjectionErrors) {
    Diagnostics.Params.emplace_back(
        diagNixError(ErrInfo.ActiveFile, *ErrObject,
                     decltype(DraftMgr)::decodeVersion(ErrInfo.Version)));
  }
  Diagnostics.WorkspaceVersion = WorkspaceVersion;
  EvalDiagnostic(Diagnostics);
  try {
    if (ShoudlEval) {
      Session->eval(I->dInstallable(), Depth);
      lspserver::log("evaluation done on worspace version: {0}",
                     WorkspaceVersion);
    }
  } catch (nix::BaseError &BE) {
    Diagnostics.Params.emplace_back(diagNixError(File, BE, std::nullopt));
  } catch (...) {
  }
  EvalDiagnostic(Diagnostics);
  IER = std::make_unique<IValueEvalResult>(std::move(ILR.Forest),
                                           std::move(Session));
}

void Server::onEvalDefinition(
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

        // If the expression evaluates to a "derivation", try to bring our user
        // to the location which defines the package.
        try {
          auto V = AST->getValue(Node);
          if (nix::nixd::isDerivation(*State, V)) {
            if (auto S = nix::nixd::attrPathStr(*State, V, "meta.position")) {
              llvm::StringRef PositionStr = S.value();
              auto [Path, LineStr] = PositionStr.split(':');
              int Line;
              if (LLVM_LIKELY(!LineStr.getAsInteger(10, Line))) {
                lspserver::Position Position{.line = Line, .character = 0};
                RR.Response = Location{URIForFile::canonicalize(Path, Path),
                                       {Position, Position}};
                return;
              }
            }
          } else {
            // There is a value avaiable, this might be useful for locations
            auto P = V.determinePos(nix::noPos);
            if (P != nix::noPos) {
              auto Pos = State->positions[P];
              if (auto *SourcePath =
                      std::get_if<nix::SourcePath>(&Pos.origin)) {
                auto Path = SourcePath->to_string();
                lspserver::Position Position = toLSPPos(State->positions[P]);
                RR.Response = Location{URIForFile::canonicalize(Path, Path),
                                       {Position, Position}};
                return;
              }
            }
          }
        } catch (...) {
          lspserver::vlog("no associated value on this expression");
        }

        // Otherwise, we try to find the location binds to the variable.
        if (const auto *EVar = dynamic_cast<const nix::ExprVar *>(Node)) {
          if (EVar->fromWith)
            return;
          auto PIdx = AST->definition(EVar);
          if (PIdx == nix::noPos)
            return;

          RR.Response = Location{Params.textDocument.uri, AST->nPair(PIdx)};
          return;
        }
        RR.Response = error("requested expression is not an ExprVar.");
        return;
      });
}

void Server::onEvalDocumentLink(
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

void Server::onEvalDocumentSymbol(
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
                         Symbols AttrSymbols;
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

void Server::onEvalHover(const lspserver::TextDocumentPositionParams &Params,
                         lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  withAST<Hover>(
      Params.textDocument.uri.file().str(), ReplyRAII<Hover>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Hover> &&RR) {
        const auto *Node = AST->lookupContainMin(Params.position);
        if (!Node)
          return;
        const auto *ExprName = getExprName(Node);
        RR.Response = Hover{{MarkupKind::Markdown, ""}, std::nullopt};
        auto &HoverText = RR.Response->contents.value;
        try {
          auto Value = AST->getValue(Node);
          std::stringstream Res{};
          nix::nixd::PrintDepth = 3;
          Value.print(IER->Session->getState()->symbols, Res);
          HoverText =
              llvm::formatv("## {0} \n Value: `{1}`", ExprName, Res.str());
        } catch (const std::out_of_range &) {
          // No such value, just reply dummy item
          std::stringstream NodeOut;
          Node->show(IER->Session->getState()->symbols, NodeOut);
          lspserver::vlog("no associated value on node {0}!", NodeOut.str());
          HoverText = llvm::formatv("`{0}`", ExprName);
        }
      });
}

void Server::onEvalCompletion(const lspserver::CompletionParams &Params,
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
      const auto *Node = AST->lookupEnd(Params.position);
      if (!Node)
        return;
      try {
        auto Value = AST->getValue(Node);
        if (Value.type() == nix::ValueType::nAttrs) {
          // Traverse attribute bindings
          for (auto Binding : *Value.attrs) {
            Items.emplace_back(
                CompletionItem{.label = State->symbols[Binding.name],
                               .kind = CompletionItemKind::Field});
          }
        }
      } catch (std::out_of_range &) {
        RR.Response = error("no associated value on requested attrset.");
        return;
      }
    } else {

      const auto *Node = AST->lookupContainMin(Params.position);
      if (!Node)
        return;

      auto FromLambdaFormals = [&]() {
        // Firstly, we check that if we are in "ExprAttrs", consider this
        // is a
        // special case for completion lambda formals
        if (!dynamic_cast<const nix::ExprAttrs *>(Node))
          return;
        // Then, check that if the parent of evaluates to a "<LAMBDA>"
        try {
          const auto *Parent = AST->parent(Node);
          if (const auto *SomeExprCall =
                  dynamic_cast<const nix::ExprCall *>(Parent)) {
            auto Value = AST->getValue(SomeExprCall->fun);
            if (!Value.isLambda())
              return;
            // Then, filling the completion list using that lambda
            // formals.
            auto *Fun = Value.lambda.fun;
            if (!Fun->hasFormals())
              return;
            for (auto Formal : Fun->formals->formals) {
              CompletionItem Item;
              Item.label = State->symbols[Formal.name];
              Item.kind = CompletionItemKind::Constructor;
              Items.emplace_back(std::move(Item));
            }
          }

        } catch (...) {
        }
      };
      FromLambdaFormals();
      try {
        const auto *Node = AST->lookupContainMin(Params.position);
        if (!Node)
          return;
        const auto *ExprEnv = AST->getEnv(Node);
        CompletionHelper::fromEnvRecursive(
            State->symbols, *State->staticBaseEnv, *ExprEnv, Items);
      } catch (std::out_of_range &) {
        CompletionHelper::fromStaticEnv(State->symbols, *State->staticBaseEnv,
                                        Items);
      }
    }
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

} // namespace nixd
