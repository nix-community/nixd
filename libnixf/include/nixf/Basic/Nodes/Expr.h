#pragma once

#include "Attrs.h"

namespace nixf {

class ExprSelect : public Expr {
  const std::shared_ptr<Expr> E;
  const std::shared_ptr<AttrPath> Path;
  const std::shared_ptr<Expr> Default;

public:
  ExprSelect(LexerCursorRange Range, std::shared_ptr<Expr> E,
             std::shared_ptr<AttrPath> Path, std::shared_ptr<Expr> Default)
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
    return {E.get(), Path.get(), Default.get()};
  }
};

/// A call/apply to some function.
class ExprCall : public Expr {
  const std::shared_ptr<Expr> Fn;
  const std::vector<std::shared_ptr<Expr>> Args;

public:
  ExprCall(LexerCursorRange Range, std::shared_ptr<Expr> Fn,
           std::vector<std::shared_ptr<Expr>> Args)
      : Expr(NK_ExprCall, Range), Fn(std::move(Fn)), Args(std::move(Args)) {
    assert(this->Fn && "Fn must not be null");
  }

  [[nodiscard]] Expr &fn() const {
    assert(Fn && "Fn must not be null");
    return *Fn;
  }

  [[nodiscard]] const std::vector<std::shared_ptr<Expr>> &args() const {
    return Args;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Args.size() + 1);
    Children.emplace_back(Fn.get());
    for (const auto &Member : Args) {
      Children.emplace_back(Member.get());
    }
    return Children;
  }
};

class ExprList : public Expr {
  const std::vector<std::shared_ptr<Expr>> Elements;

public:
  ExprList(LexerCursorRange Range, std::vector<std::shared_ptr<Expr>> Elements)
      : Expr(NK_ExprList, Range), Elements(std::move(Elements)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<Expr>> &elements() const {
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

class ExprIf : public Expr {
  const std::shared_ptr<Expr> Cond;
  const std::shared_ptr<Expr> Then;
  const std::shared_ptr<Expr> Else;

public:
  ExprIf(LexerCursorRange Range, std::shared_ptr<Expr> Cond,
         std::shared_ptr<Expr> Then, std::shared_ptr<Expr> Else)
      : Expr(NK_ExprIf, Range), Cond(std::move(Cond)), Then(std::move(Then)),
        Else(std::move(Else)) {}

  [[nodiscard]] Expr *cond() const { return Cond.get(); }
  [[nodiscard]] Expr *then() const { return Then.get(); }
  [[nodiscard]] Expr *elseExpr() const { return Else.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {Cond.get(), Then.get(), Else.get()};
  }
};

class ExprAssert : public Expr {
  const std::shared_ptr<Expr> Cond;
  const std::shared_ptr<Expr>
      Value; // If "cond" is true, then "value" is returned.

public:
  ExprAssert(LexerCursorRange Range, std::shared_ptr<Expr> Cond,
             std::shared_ptr<Expr> Value)
      : Expr(NK_ExprAssert, Range), Cond(std::move(Cond)),
        Value(std::move(Value)) {}

  [[nodiscard]] Expr *cond() const { return Cond.get(); }
  [[nodiscard]] Expr *value() const { return Value.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {Cond.get(), Value.get()};
  }
};

class ExprLet : public Expr {
  // 'let' binds 'in' expr

  const std::shared_ptr<Misc> KwLet; // 'let', not null
  const std::shared_ptr<Misc> KwIn;
  const std::shared_ptr<Expr> E;

  const std::shared_ptr<ExprAttrs> Attrs;

public:
  ExprLet(LexerCursorRange Range, std::shared_ptr<Misc> KwLet,
          std::shared_ptr<Misc> KwIn, std::shared_ptr<Expr> E,
          std::shared_ptr<ExprAttrs> Attrs)
      : Expr(NK_ExprLet, Range), KwLet(std::move(KwLet)), KwIn(std::move(KwIn)),
        E(std::move(E)), Attrs(std::move(Attrs)) {
    assert(this->KwLet && "KwLet should not be empty!");
  }

  [[nodiscard]] const Binds *binds() const {
    return Attrs ? Attrs->binds() : nullptr;
  }
  [[nodiscard]] const ExprAttrs *attrs() const { return Attrs.get(); }
  [[nodiscard]] const Expr *expr() const { return E.get(); }
  [[nodiscard]] const Misc &let() const { return *KwLet; }
  [[nodiscard]] const Misc *in() const { return KwIn.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {KwLet.get(), Attrs.get(), KwIn.get(), E.get()};
  }
};

class ExprWith : public Expr {
  const std::shared_ptr<Misc> KwWith;
  const std::shared_ptr<Misc> TokSemi;
  const std::shared_ptr<Expr> With;
  const std::shared_ptr<Expr> E;

public:
  ExprWith(LexerCursorRange Range, std::shared_ptr<Misc> KwWith,
           std::shared_ptr<Misc> TokSemi, std::shared_ptr<Expr> With,
           std::shared_ptr<Expr> E)
      : Expr(NK_ExprWith, Range), KwWith(std::move(KwWith)),
        TokSemi(std::move(TokSemi)), With(std::move(With)), E(std::move(E)) {}

  [[nodiscard]] const Misc &kwWith() const { return *KwWith; }
  [[nodiscard]] const Misc *tokSemi() const { return TokSemi.get(); }
  [[nodiscard]] Expr *with() const { return With.get(); }
  [[nodiscard]] Expr *expr() const { return E.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {KwWith.get(), TokSemi.get(), With.get(), E.get()};
  }

  [[nodiscard]] std::vector<std::string> selector() const {
    std::vector<std::string> Parts;
    for (const auto &Child : With->children()) {
      if (!Child)
        continue;
      switch (Child->kind()) {
      case Node::NK_ExprVar: {
        const auto &Var = static_cast<const ExprVar &>(*Child);
        Parts.emplace_back(Var.id().name());
        break;
      }
      case Node::NK_AttrPath: {
        const auto &Path = static_cast<const AttrPath &>(*Child);
        for (const auto &Name : Path.names())
          Parts.emplace_back(Name->id()->name());
        break;
      }
      default:
        break;
      }
    }
    return Parts;
  }
};

} // namespace nixf
