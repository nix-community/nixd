#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"
#include "nixd/Diagnostic.h"
#include "nixd/Expr.h"

#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"

#include <nix/nixexpr.hh>

#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>

namespace nixd {

EvalAST::EvalAST(nix::Expr *Root) : Cxt(ASTContext()) {
  auto EvalCallback = [this](const nix::Expr *Expr, const nix::EvalState &,
                             const nix::Env &ExprEnv,
                             const nix::Value &ExprValue) {
    ValueMap[Expr] = ExprValue;
    EnvMap[Expr] = &ExprEnv;
  };
  this->Root = rewriteCallback(Cxt, EvalCallback, Root);
}

void EvalAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) {
  nix::Value DummyValue{};
  State.cacheFile(Path.str(), Path.str(), Root, DummyValue);
}

nix::Value EvalAST::searchUpValue(const nix::Expr *Expr) const {
  for (;;) {
    if (ValueMap.contains(Expr))
      return ValueMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  throw std::out_of_range("No such value associated to ancestors");
}

const nix::Env *EvalAST::searchUpEnv(const nix::Expr *Expr) const {
  for (;;) {
    if (EnvMap.contains(Expr))
      return EnvMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  throw std::out_of_range("No such env associated to ancestors");
}

nix::Expr *EvalAST::lookupPosition(lspserver::Position Pos) const {
  auto WordHead = PosMap.upper_bound(Pos);
  if (WordHead != PosMap.begin())
    WordHead--;
  return Cxt.Nodes[WordHead->second].get();
}

void EvalAST::preparePositionLookup(const nix::EvalState &State) {
  assert(PosMap.empty() &&
         "PosMap must be empty before preparing! (prepared before?)");
  // Reversely traversing AST nodes, to make sure we found "larger" AST nodes
  // before smaller
  // e.g. ExprSelect = (some).foo.bar
  //       ExprVar   = some
  // "lookupPosition" should return ExprSelect, instead of ExprVar.
  for (size_t Idx = Cxt.Nodes.size(); Idx-- > 0;) {
    auto PosIdx = Cxt.Nodes[Idx]->getPos();
    if (PosIdx == nix::noPos)
      continue;
    nix::Pos Pos = State.positions[PosIdx];
    PosMap.insert({translatePosition(Pos), Idx});
  }
}

} // namespace nixd
