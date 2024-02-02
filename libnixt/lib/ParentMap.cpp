#include "nixt/ParentMap.h"

namespace nixt {

ParentMap parentMap(const nix::Expr *Root) {
  ParentMap Ret;
  struct VisitorClass : RecursiveASTVisitor<VisitorClass> {
    // The parent before traverseExpr
    const nix::Expr *ParentExpr;
    ParentMap *CapturedRet;

    bool traverseExpr(const nix::Expr *E) {
      CapturedRet->insert({E, ParentExpr});
      const auto *OldParent = ParentExpr;
      ParentExpr = E; // Set the parent into the visitor, it should be the
                      // parent when we are traversing child nodes.
      if (!RecursiveASTVisitor<VisitorClass>::traverseExpr(E))
        return false;

      // After traversing on childrens finished, set parent expr to previous
      // parent.
      ParentExpr = OldParent;
      return true;
    }

  } Visitor;

  Visitor.ParentExpr = Root;
  Visitor.CapturedRet = &Ret;

  Visitor.traverseExpr(Root);

  return Ret;
}

} // namespace nixt
