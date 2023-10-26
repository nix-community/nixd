/// SyntaxKinds.inc, declares syntax names.

/// provides: EXPR(Name)  - expressions. They can be evaluated.
/// provides: NODE(Name)  - simple node. They cannot be evaluated.

// Expressions.
#ifdef EXPR
EXPR(Assert)
EXPR(AttrSet)
EXPR(Call)
EXPR(Float)
EXPR(Int)
EXPR(If)
EXPR(Lambda)
EXPR(Let)
EXPR(List)
EXPR(OpNegate)
EXPR(OpAnd)
EXPR(OpConcatLists)
EXPR(OpEq)
EXPR(OpHasAttr)
EXPR(OpImpl)
EXPR(OpNEq)
EXPR(OpNot)
EXPR(OpOr)
EXPR(OpUpdate)
EXPR(Path)
EXPR(Select)
EXPR(String)
EXPR(Var)
EXPR(With)
EXPR(Paren)
// let { ..., .body }, desugared to rec { ..., .body }.body
EXPR(LegacyLet)
#endif // EXPR

#ifdef NODE
NODE(Token)
NODE(TriviaPiece)
NODE(Trivia)
NODE(ID)
NODE(Formals)
NODE(Formal)
NODE(Interpolation)
NODE(AttrPath)
NODE(Inherit)
NODE(Binds)
NODE(Binding)
NODE(ListBody)
NODE(LambdaArg)
NODE(BracedFormals)
NODE(Unknown)
NODE(Root)
#endif // NODE
