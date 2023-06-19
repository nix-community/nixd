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

#include <algorithm>
#include <exception>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nixd {

void Server::switchToEvaluator(lspserver::PathRef File) {
  initWorker();
  Role = ServerRole::Evaluator;
  EvalDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");

  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &Server::onEvalCompletion);

  Registry.addMethod("nixd/ipc/textDocument/definition", this,
                     &Server::onEvalDefinition);

  evalInstallable(File, Config.getEvalDepth());
  mkOutNotifiction<ipc::WorkerMessage>("nixd/ipc/finished")(
      ipc::WorkerMessage{WorkspaceVersion});
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
    insertDiagnostic(*ErrObject, Diagnostics.Params,
                     decltype(DraftMgr)::decodeVersion(ErrInfo.Version));
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
    insertDiagnostic(BE, Diagnostics.Params);
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

      // TODO: share the same code with static worker
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
