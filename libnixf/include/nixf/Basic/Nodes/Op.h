#pragma once

#include "Basic.h"

#include "nixf/Basic/TokenKinds.h"

#include <memory>

namespace nixf {

class Op : public Node {
  tok::TokenKind OpKind;

public:
  Op(LexerCursorRange Range, tok::TokenKind OpKind)
      : Node(NK_Op, Range), OpKind(OpKind) {}

  [[nodiscard]] tok::TokenKind op() const { return OpKind; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

/// \brief Abstract class for binary operators and unary operators.
class ExprOp : public Expr {
  std::unique_ptr<Op> O;

public:
  ExprOp(NodeKind Kind, LexerCursorRange Range, std::unique_ptr<Op> O)
      : Expr(Kind, Range), O(std::move(O)) {
    assert(this->O && "O must not be null");
  }

  [[nodiscard]] Op &op() const { return *O; }

  [[nodiscard]] ChildVector children() const override { return {O.get()}; }
};

class ExprBinOp : public ExprOp {
  std::unique_ptr<Expr> LHS;
  std::unique_ptr<Expr> RHS;

public:
  ExprBinOp(LexerCursorRange Range, std::unique_ptr<Op> O,
            std::unique_ptr<Expr> LHS, std::unique_ptr<Expr> RHS)
      : ExprOp(NK_ExprBinOp, Range, std::move(O)), LHS(std::move(LHS)),
        RHS(std::move(RHS)) {}

  [[nodiscard]] Expr *lhs() const { return LHS.get(); }
  [[nodiscard]] Expr *rhs() const { return RHS.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {&op(), LHS.get(), RHS.get()};
  }
};

class ExprUnaryOp : public ExprOp {
  std::unique_ptr<Expr> E;

public:
  ExprUnaryOp(LexerCursorRange Range, std::unique_ptr<Op> O,
              std::unique_ptr<Expr> E)
      : ExprOp(NK_ExprUnaryOp, Range, std::move(O)), E(std::move(E)) {}

  [[nodiscard]] Expr *expr() const { return E.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {&op(), E.get()};
  }
};

} // namespace nixf
