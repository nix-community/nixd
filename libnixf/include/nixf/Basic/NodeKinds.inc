/// provides NODE(NAME) EXPR(NAME)

#ifdef NODE

/// ${ expr }
NODE(Interpolation)
NODE(InterpolableParts)

/// \brief Misc node used for parentheses keywords etc.
/// \see Misc
NODE(Misc)
NODE(Identifer)
NODE(AttrName)
NODE(AttrPath)
NODE(Binding)
NODE(Inherit)
NODE(Binds)
NODE(LambdaArg)
NODE(Formals)
NODE(Formal)
NODE(Op)

#endif // NODE



#ifdef EXPR

EXPR(ExprInt)
EXPR(ExprFloat)
EXPR(ExprVar)
EXPR(ExprString)
EXPR(ExprPath)
EXPR(ExprParen)
EXPR(ExprAttrs)
EXPR(ExprSelect)
EXPR(ExprCall)
EXPR(ExprList)
EXPR(ExprLambda)
EXPR(ExprBinOp)
EXPR(ExprUnaryOp)
EXPR(ExprIf)

#endif // EXPR
