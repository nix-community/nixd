#include "nixd/Sema/CompletionBuilder.h"
#include "nixd/AST/ParseAST.h"
#include "nixd/Nix/Eval.h"

#include "lspserver/Protocol.h"

#include <stdexcept>

namespace nixd {

using lspserver::CompletionItem;
using lspserver::CompletionItemKind;

void CompletionBuilder::addAttrFields(const EvalAST &AST,
                                      const lspserver::Position &Pos,
                                      nix::EvalState &State) {
  try {
    if (const auto *Node = AST.lookupEnd(Pos)) {
      auto Value = AST.getValueEval(Node, State);
      if (Value.type() == nix::ValueType::nAttrs) {
        // Traverse attribute bindings
        for (auto Binding : *Value.attrs) {
          CompletionItem R;
          R.label = State.symbols[Binding.name];
          R.kind = CompletionItemKind::Field;
          Result.items.emplace_back(std::move(R));
          if (Result.items.size() > Limit) {
            Result.isIncomplete = true;
            break;
          }
        }
      }
    }
  } catch (std::out_of_range &) {
  }
}

void CompletionBuilder::addSymbols(const ParseAST &AST, const nix::Expr *Node) {
  std::vector<nix::Symbol> Symbols;
  AST.collectSymbols(Node, Symbols);
  // Insert symbols to our completion list.
  std::transform(Symbols.begin(), Symbols.end(),
                 std::back_inserter(Result.items), [&](const nix::Symbol &V) {
                   lspserver::CompletionItem R;
                   R.kind = CompletionItemKind::Interface;
                   R.label = AST.symbols()[V];
                   return R;
                 });
}

void CompletionBuilder::addLambdaFormals(const EvalAST &AST,
                                         nix::EvalState &State,
                                         const nix::Expr *Node) {

  // Nix only accepts attr set formals
  if (!dynamic_cast<const nix::ExprAttrs *>(Node))
    return;

  try {
    const auto *Parent = AST.parent(Node);
    if (const auto *SomeExprCall =
            dynamic_cast<const nix::ExprCall *>(Parent)) {
      auto Value = AST.getValueEval(SomeExprCall->fun, State);
      if (!Value.isLambda())
        return;
      auto *Fun = Value.lambda.fun;
      if (!Fun->hasFormals())
        return;
      for (auto Formal : Fun->formals->formals) {
        CompletionItem R;
        R.label = State.symbols[Formal.name];
        R.kind = CompletionItemKind::Constructor;
        Result.items.emplace_back(std::move(R));
      }
    }
  } catch (std::out_of_range &) {
  }
}

void CompletionBuilder::addEnv(nix::EvalState &State, nix::Env &NixEnv) {
  if (NixEnv.type == nix::Env::HasWithExpr) {
    nix::Value *V = State.allocValue();
    State.evalAttrs(*NixEnv.up, (nix::Expr *)NixEnv.values[0], *V, nix::noPos,
                    "<borked>");
    NixEnv.values[0] = V;
    NixEnv.type = nix::Env::HasWithAttrs;
  }
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    for (const auto &SomeAttr : *NixEnv.values[0]->attrs) {
      std::string Name = State.symbols[SomeAttr.name];
      lspserver::CompletionItem R;
      R.label = Name;
      R.kind = lspserver::CompletionItemKind::Variable;
      Result.items.emplace_back(std::move(R));

      if (Result.items.size() > Limit) {
        Result.isIncomplete = true;
        break;
      }
    }
  }
  if (NixEnv.up)
    addEnv(State, *NixEnv.up);
}

void CompletionBuilder::addEnv(const EvalAST &AST, nix::EvalState &State,
                               const nix::Expr *Node) {
  try {
    // Eval this node, for reaching deeper envs (e.g. with expressions).
    AST.getValueEval(Node, State);
    if (auto *ExprEnv = AST.searchUpEnv(Node))
      addEnv(State, *ExprEnv);
  } catch (std::out_of_range &) {
  }
}

void CompletionBuilder::addStaticEnv(const nix::SymbolTable &STable,
                                     const nix::StaticEnv &SEnv) {
  for (auto [Symbol, Displ] : SEnv.vars) {
    std::string Name = STable[Symbol];
    if (Name.starts_with("__"))
      continue;

    CompletionItem R;
    R.label = Name;
    R.kind = CompletionItemKind::Constant;
    Result.items.emplace_back(std::move(R));
  }

  if (SEnv.up)
    addStaticEnv(STable, SEnv);
}

} // namespace nixd
