/// provides NODE(NAME) EXPR(NAME)

#ifdef NODE
NODE(InterpolableParts)

/// \brief Misc node used for parentheses keywords etc.
/// \see Misc
NODE(Misc)
NODE(Identifer)
NODE(AttrName)
NODE(AttrPath)
NODE(Binding)
NODE(Binds)

#endif // NODE



#ifdef EXPR

EXPR(ExprInt)
EXPR(ExprFloat)
EXPR(ExprVar)
EXPR(ExprString)
EXPR(ExprPath)
EXPR(ExprParen)
EXPR(ExprAttrs)

#endif // EXPR
