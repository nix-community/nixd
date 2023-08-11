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
  const nix::Expr *Node = AST.lookupEnd(Pos);

  if (!Node)
    return;

  auto OpV = AST.getValueEval(Node, State);

  if (!OpV || OpV->type() != nix::ValueType::nAttrs)
    return;

  for (const auto &Attr : *OpV->attrs) {

    CompletionItem R;
    R.label = State.symbols[Attr.name];
    R.kind = CompletionItemKind::Field;
    Result.items.emplace_back(std::move(R));

    if (Result.items.size() > Limit) {
      Result.isIncomplete = true;
      break;
    }
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

  // Matching this structure:
  //
  //   *lambda* { | }
  //              ^

  const auto *ParentCall =
      dynamic_cast<const nix::ExprCall *>(AST.parent(Node));
  if (!ParentCall)
    return;

  auto OpVFunc = AST.getValueEval(ParentCall->fun, State);
  if (!OpVFunc || !OpVFunc->isLambda())
    return;

  auto *EL = OpVFunc->lambda.fun;
  if (!EL->hasFormals())
    return;

  for (const auto &Formal : EL->formals->formals) {
    CompletionItem R;
    R.label = State.symbols[Formal.name];
    R.kind = CompletionItemKind::Constructor;
    Result.items.emplace_back(std::move(R));
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
  // Eval this node, for reaching deeper envs (e.g. with expressions).
  AST.getValueEval(Node, State);
  if (auto *ExprEnv = AST.searchUpEnv(Node))
    addEnv(State, *ExprEnv);
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
