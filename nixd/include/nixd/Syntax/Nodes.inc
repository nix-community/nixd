#ifdef NODE

#ifndef COMMON_METHOD
#define COMMON_METHOD
#define P_COMMON_METHOD_DEFINED
#endif

NODE(Identifier, {
  nix::Symbol Symbol;
  COMMON_METHOD
})
NODE(Formal, {
  Identifier *ID;
  /// The default argument.
  Node *Default;
  COMMON_METHOD
})
NODE(Formals, {
  std::vector<Formal *> Formals;
  bool Ellipsis;
  COMMON_METHOD
})
NODE(Function, {
  Identifier *Arg;
  Formals *FunctionFormals;
  Node *Body;
  COMMON_METHOD
})
NODE(Assert, {
  Node *Cond;
  Node *Body;
  COMMON_METHOD
})
NODE(With, {
  Node *Attrs;
  Node *Body;
  COMMON_METHOD
})
NODE(Binds, {
  std::vector<Node *> Attributes;
  COMMON_METHOD
})

NODE(ListBody, {
  std::vector<Node *> Elems;
  COMMON_METHOD
})
NODE(List, {
  ListBody *Body;
  COMMON_METHOD
})

NODE(Let, {
  syntax::Binds *Binds;
  Node *Body;
  COMMON_METHOD
})
NODE(If, {
  Node *Cond;
  Node *Then;
  Node *Else;
  COMMON_METHOD
})
NODE(Variable, {
  Identifier *ID;
  COMMON_METHOD
})
NODE(Int, {
  nix::NixInt N;
  COMMON_METHOD
})
NODE(Float, {
  nix::NixFloat NF;
  COMMON_METHOD
})
NODE(InterpExpr, {
  Node *Body;
  COMMON_METHOD
})
NODE(String, {
  std::string S;
  bool HasIndentation;
  COMMON_METHOD
})
NODE(IndString, {
  std::string S;
  bool HasIndentation;
  COMMON_METHOD
})
NODE(IndStringParts, {
  std::vector<Node *> SubStrings;
  COMMON_METHOD
})
NODE(ConcatStrings, {
  std::vector<Node *> SubStrings;
  // nix::ExprConcatStrings::forceString
  bool ForceString;
  COMMON_METHOD
})
NODE(HPath, {
  std::string S;
  COMMON_METHOD
})
NODE(Path, {
  std::string S;
  COMMON_METHOD
})
NODE(SPath, {
  std::string S;
  COMMON_METHOD
})
NODE(URI, {
  std::string S;
  COMMON_METHOD
})

NODE(Call, {
  Node *Fn;
  std::vector<Node *> Args;
  COMMON_METHOD
})
NODE(AttrPath, {
  std::vector<Node *> Names;
  COMMON_METHOD
})
NODE(Attrs, {
  std::vector<Node *> Names;
  COMMON_METHOD
})

NODE(Attribute, {
  AttrPath *Path;
  Node *Body;
  COMMON_METHOD
})

NODE(InheritedAttribute, {
  Attrs *InheritedAttrs;
  Node *E;
  COMMON_METHOD
})

NODE(Select, {
  Node *Body;

  AttrPath *Path;

  /// expr1 '.' attrpath 'or' expr2
  /// expr2 is the default value if we cannot select
  Node *Default;
  COMMON_METHOD
})

NODE(OpHasAttr, {
  Node *Operand;
  AttrPath *Path;
  RangeIdx OpRange;
  COMMON_METHOD
})
NODE(OpNot, {
  Node *Body;
  RangeIdx OpRange;
  COMMON_METHOD
})
NODE(OpNegate, {
  Node *Body;
  RangeIdx OpRange;
  COMMON_METHOD
})

NODE(Braced, {
  Node *Body;
  COMMON_METHOD
})

NODE(AttrSet, {
  Binds *AttrBinds;
  bool Recursive;
  COMMON_METHOD
})

NODE(LegacyLet, {
  Binds *AttrBinds;
  COMMON_METHOD
})

// Binary Operators
#define BIN_OP(NAME, _)                                                        \
  NODE(NAME, {                                                                 \
    Node *LHS;                                                                 \
    Node *RHS;                                                                 \
    RangeIdx OpRange;                                                          \
    COMMON_METHOD                                                              \
  })
#include "BinaryOps.inc"
#undef BIN_OP

#ifdef P_COMMON_METHOD_DEFINED
#undef COMMON_METHOD
#undef P_COMMON_METHOD_DEFINED
#endif // P_COMMON_METHOD_DEFINED

#endif // NODE
