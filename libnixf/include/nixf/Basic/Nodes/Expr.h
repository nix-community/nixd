#pragma once

#include "Attrs.h"

namespace nixf {

class ExprSelect : public Expr {
  std::unique_ptr<Expr> E;
  std::unique_ptr<AttrPath> Path;
  std::unique_ptr<Expr> Default;

public:
  ExprSelect(LexerCursorRange Range, std::unique_ptr<Expr> E,
             std::unique_ptr<AttrPath> Path, std::unique_ptr<Expr> Default)
      : Expr(NK_ExprSelect, Range), E(std::move(E)), Path(std::move(Path)),
        Default(std::move(Default)) {
    assert(this->E && "E must not be null");
  }

  [[nodiscard]] Expr &expr() const {
    assert(E && "E must not be null");
    return *E;
  }

  [[nodiscard]] Expr *defaultExpr() const { return Default.get(); }

  [[nodiscard]] AttrPath *path() const { return Path.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {E.get(), Path.get()};
  }
};

/// A call/apply to some function.
class ExprCall : public Expr {
  std::unique_ptr<Expr> Fn;
  std::vector<std::unique_ptr<Expr>> Args;

public:
  ExprCall(LexerCursorRange Range, std::unique_ptr<Expr> Fn,
           std::vector<std::unique_ptr<Expr>> Args)
      : Expr(NK_ExprCall, Range), Fn(std::move(Fn)), Args(std::move(Args)) {
    assert(this->Fn && "Fn must not be null");
  }

  [[nodiscard]] Expr &fn() const {
    assert(Fn && "Fn must not be null");
    return *Fn;
  }
  std::vector<std::unique_ptr<Expr>> &args() { return Args; }

  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &args() const {
    return Args;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Args.size());
    for (const auto &Member : Args) {
      Children.emplace_back(Member.get());
    }
    return Children;
  }
};

class ExprList : public Expr {
  std::vector<std::unique_ptr<Expr>> Elements;

public:
  ExprList(LexerCursorRange Range, std::vector<std::unique_ptr<Expr>> Elements)
      : Expr(NK_ExprList, Range), Elements(std::move(Elements)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &elements() const {
    return Elements;
  }

  [[nodiscard]] std::vector<std::unique_ptr<Expr>> &elements() {
    return Elements;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Elements.size());
    for (const auto &Element : Elements) {
      Children.emplace_back(Element.get());
    }
    return Children;
  }
};

} // namespace nixf
