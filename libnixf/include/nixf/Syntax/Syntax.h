#pragma once

namespace nixf {

/// Abstract class for all expressions
struct ExprSyntax {
  enum ExprKind {
    EK_Formal,
    EK_Formals,
    EK_Lambda,
    EK_Assert,
    EK_With,
    EK_Binds,
    EK_List,
    EK_Let,
    EK_If,
    EK_Variable,
    EK_Int,
    EK_Float,
    EK_String,
  } Kind;
  ExprSyntax(ExprKind Kind) : Kind(Kind) {}
};

} // namespace nixf
