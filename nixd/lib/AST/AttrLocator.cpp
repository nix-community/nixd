#include "nixd/AST/AttrLocator.h"
#include "nixd/AST/ParseAST.h"
#include "nixd/Expr/Expr.h"

namespace nixd {

AttrLocator::AttrLocator(const ParseAST &AST) {
  // Traverse the AST, record all attr locations
  // We can assume that Nix attrs are not interleaving
  // i.e. they are concrete tokens.
  struct VTy : RecursiveASTVisitor<VTy> {
    AttrLocator &This;
    const ParseAST &AST;
    bool visitExprAttrs(const nix::ExprAttrs *EA) {
      for (const auto &[Symbol, Attr] : EA->attrs) {
        lspserver::Range Range = AST.nPair(Attr.pos);
        // Note: "a.b.c" will be assigned more than once
        // Our visitor model ensures the order that descendant nodes will be
        // visited at last, for nesting attrs.
        This.Attrs[Range] = Attr.e;
      }
      return true;
    }
  } V{.This = *this, .AST = AST};
  V.traverseExpr(AST.root());
}

const nix::Expr *AttrLocator::locate(lspserver::Position Pos) const {
  auto It = Attrs.upper_bound({Pos, {INT_MAX, INT_MAX}});
  if (It == Attrs.begin())
    return nullptr;
  --It;
  const auto &[Range, Expr] = *It;

  // Check that the range actually contains the desired position.
  // Otherwise, we should return nullptr, denoting the position is not covered
  // by any knwon attrs.
  return Range.contains(Pos) ? Expr : nullptr;
}

} // namespace nixd
