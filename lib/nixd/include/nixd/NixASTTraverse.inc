DEF_TRAVERSE_TYPE(ExprAssert, {
  TRY_TO_TRAVERSE(T->cond);
  TRY_TO_TRAVERSE(T->body);
})

DEF_TRAVERSE_TYPE(ExprAttrs, {
  for (auto &[_, Elem] : T->attrs)
    TRY_TO_TRAVERSE(Elem.e);
  for (auto &DAD : T->dynamicAttrs) {
    TRY_TO_TRAVERSE(DAD.nameExpr);
    TRY_TO_TRAVERSE(DAD.valueExpr);
  }
})

DEF_TRAVERSE_TYPE(ExprCall, {
  for (auto &Arg : T->args)
    TRY_TO_TRAVERSE(Arg);
  TRY_TO_TRAVERSE(T->fun);
})

DEF_TRAVERSE_TYPE(ExprConcatStrings, {
  for (auto &[_, E] : *T->es)
    TRY_TO_TRAVERSE(E);
})

DEF_TRAVERSE_TYPE(ExprFloat, {})

DEF_TRAVERSE_TYPE(ExprIf, {
  TRY_TO_TRAVERSE(T->cond);
  TRY_TO_TRAVERSE(T->then);
  TRY_TO_TRAVERSE(T->else_);
})

DEF_TRAVERSE_TYPE(ExprInt, {})

DEF_TRAVERSE_TYPE(ExprLambda, {
  if (T->hasFormals())
    for (auto &F : T->formals->formals)
      TRY_TO_TRAVERSE(F.def);
  TRY_TO_TRAVERSE(T->body);
})

DEF_TRAVERSE_TYPE(ExprLet, {
  TRY_TO_TRAVERSE(T->attrs);
  TRY_TO_TRAVERSE(T->body);
})

DEF_TRAVERSE_TYPE(ExprList, {
  for (auto &E : T->elems)
    TRY_TO_TRAVERSE(E);
})

#define DEF_TRAVERSE_BINARY_EXPR(BIN_OP)                                       \
  DEF_TRAVERSE_TYPE(BIN_OP, {                                                  \
    TRY_TO_TRAVERSE(T->e1);                                                    \
    TRY_TO_TRAVERSE(T->e2);                                                    \
  })

DEF_TRAVERSE_BINARY_EXPR(ExprOpAnd)
DEF_TRAVERSE_BINARY_EXPR(ExprOpConcatLists)
DEF_TRAVERSE_BINARY_EXPR(ExprOpEq)
DEF_TRAVERSE_BINARY_EXPR(ExprOpImpl)
DEF_TRAVERSE_BINARY_EXPR(ExprOpNEq)
DEF_TRAVERSE_BINARY_EXPR(ExprOpOr)
DEF_TRAVERSE_BINARY_EXPR(ExprOpUpdate)

#undef DEF_TRAVERSE_BINARY_EXPR

DEF_TRAVERSE_TYPE(ExprOpHasAttr, {
  TRY_TO_TRAVERSE(T->e);
  for (auto &E : T->attrPath)
    if (!E.symbol)
      TRY_TO_TRAVERSE(E.expr);
})

DEF_TRAVERSE_TYPE(ExprOpNot, { TRY_TO_TRAVERSE(T->e); })

DEF_TRAVERSE_TYPE(ExprPath, {})
DEF_TRAVERSE_TYPE(ExprPos, {})
DEF_TRAVERSE_TYPE(ExprSelect, {
  TRY_TO_TRAVERSE(T->def);
  TRY_TO_TRAVERSE(T->e);
  for (auto &E : T->attrPath) {
    if (!E.symbol)
      TRY_TO_TRAVERSE(E.expr);
  }
})

DEF_TRAVERSE_TYPE(ExprString, {})
DEF_TRAVERSE_TYPE(ExprVar, {})
DEF_TRAVERSE_TYPE(ExprWith, {
  TRY_TO_TRAVERSE(T->attrs);
  TRY_TO_TRAVERSE(T->body);
})
