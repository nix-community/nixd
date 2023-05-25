#include "nixd/AST.h"
#include "lspserver/Protocol.h"
#include "nixd/CallbackExpr.h"

#include "lspserver/Logger.h"
#include "nixd/Diagnostic.h"
#include "nixd/Expr.h"
#include "nixexpr.hh"

#include <cstddef>
#include <exception>
#include <memory>

namespace nixd {

EvalAST::EvalAST(nix::Expr *Root) : Cxt(ASTContext()) {
  auto EvalCallback = [this](const nix::Expr *Expr, const nix::EvalState &,
                             const nix::Env &ExprEnv,
                             const nix::Value &ExprValue) {
    ValueMap[Expr] = ExprValue;
    EnvMap[Expr] = ExprEnv;
  };
  this->Root = rewriteCallback(Cxt, EvalCallback, Root);
}

void EvalAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) {
  nix::Value DummyValue{};
  State.cacheFile(Path.str(), Path.str(), Root, DummyValue);
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
  for (size_t Idx = 0; Idx < Cxt.Nodes.size(); Idx++) {
    auto PosIdx = Cxt.Nodes[Idx]->getPos();
    if (PosIdx == nix::noPos)
      continue;
    nix::Pos Pos = State.positions[PosIdx];
    PosMap[translatePosition(Pos)] = Idx;
  }
}

} // namespace nixd
