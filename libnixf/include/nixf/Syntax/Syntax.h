#pragma once

#include <utility>

#include "SyntaxView.h"
#include "nixf/Syntax/RawSyntax.h"

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

struct VarSyntax : ExprSyntax {
  VarSyntax(std::shared_ptr<RawSyntax> Raw)
      : ExprSyntax(EK_Variable), Var(std::move(Raw)) {}
  SyntaxView Var;
};

struct IntSyntax : ExprSyntax {
  SyntaxView Int;
  IntSyntax(std::shared_ptr<RawSyntax> Raw)
      : ExprSyntax(EK_Int), Int(std::move(Raw)) {}
};

struct FloatSyntax : ExprSyntax {
  SyntaxView Int;
  FloatSyntax(std::shared_ptr<RawSyntax> Raw)
      : ExprSyntax(EK_Int), Int(std::move(Raw)) {}
};

struct IfSyntax : ExprSyntax {
  std::shared_ptr<ExprSyntax> Cond;
  std::shared_ptr<ExprSyntax> Body;
};

} // namespace nixf
