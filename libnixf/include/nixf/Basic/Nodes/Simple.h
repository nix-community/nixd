#pragma once

#include "Basic.h"

#include <boost/container/small_vector.hpp>

#include <vector>

namespace nixf {

using NixInt = int64_t;
using NixFloat = double;

class ExprInt : public Expr {
  NixInt Value;

public:
  ExprInt(LexerCursorRange Range, NixInt Value)
      : Expr(NK_ExprInt, Range), Value(Value) {}
  [[nodiscard]] NixInt value() const { return Value; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprFloat : public Expr {
  NixFloat Value;

public:
  ExprFloat(LexerCursorRange Range, NixFloat Value)
      : Expr(NK_ExprFloat, Range), Value(Value) {}
  [[nodiscard]] NixFloat value() const { return Value; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

/// \brief `${expr}` construct
class Interpolation : public Node {
  std::unique_ptr<Expr> E;

public:
  Interpolation(LexerCursorRange Range, std::unique_ptr<Expr> E)
      : Node(NK_Interpolation, Range), E(std::move(E)) {}

  [[nodiscard]] Expr *expr() const { return E.get(); }

  [[nodiscard]] ChildVector children() const override { return {E.get()}; }
};

class InterpolablePart {
public:
  enum InterpolablePartKind {
    SPK_Escaped,
    SPK_Interpolation,
  };

private:
  InterpolablePartKind Kind;
  std::string Escaped;
  std::unique_ptr<Interpolation> Interp;

public:
  explicit InterpolablePart(std::string Escaped)
      : Kind(SPK_Escaped), Escaped(std::move(Escaped)), Interp(nullptr) {}

  explicit InterpolablePart(std::unique_ptr<Interpolation> Interp)
      : Kind(SPK_Interpolation), Interp(std::move(Interp)) {
    assert(this->Interp && "interpolation must not be null");
  }

  [[nodiscard]] InterpolablePartKind kind() const { return Kind; }

  [[nodiscard]] const std::string &escaped() const {
    assert(Kind == SPK_Escaped);
    return Escaped;
  }

  [[nodiscard]] Interpolation &interpolation() const {
    assert(Kind == SPK_Interpolation);
    assert(Interp && "interpolation must not be null");
    return *Interp;
  }
};

class InterpolatedParts : public Node {
  std::vector<InterpolablePart> Fragments;

public:
  InterpolatedParts(LexerCursorRange Range,
                    std::vector<InterpolablePart> Fragments);

  [[nodiscard]] const std::vector<InterpolablePart> &fragments() const {
    return Fragments;
  };

  [[nodiscard]] bool isLiteral() const {
    return Fragments.size() == 1 &&
           Fragments[0].kind() == InterpolablePart::SPK_Escaped;
  }

  [[nodiscard]] const std::string &literal() const {
    assert(isLiteral() && "must be a literal");
    return Fragments[0].escaped();
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprString : public Expr {
  std::unique_ptr<InterpolatedParts> Parts;

public:
  ExprString(LexerCursorRange Range, std::unique_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprString, Range), Parts(std::move(Parts)) {
    assert(this->Parts && "parts must not be null");
  }

  [[nodiscard]] const InterpolatedParts &parts() const {
    assert(Parts && "parts must not be null");
    return *Parts;
  }

  [[nodiscard]] bool isLiteral() const {
    assert(Parts && "parts must not be null");
    return Parts->isLiteral();
  }

  [[nodiscard]] const std::string &literal() const {
    assert(Parts && "parts must not be null");
    return Parts->literal();
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprPath : public Expr {
  std::unique_ptr<InterpolatedParts> Parts;

public:
  ExprPath(LexerCursorRange Range, std::unique_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprPath, Range), Parts(std::move(Parts)) {
    assert(this->Parts && "parts must not be null");
  }

  [[nodiscard]] const InterpolatedParts &parts() const {
    assert(Parts && "parts must not be null");
    return *Parts;
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprParen : public Expr {
  std::unique_ptr<Expr> E;
  std::unique_ptr<Misc> LParen;
  std::unique_ptr<Misc> RParen;

public:
  ExprParen(LexerCursorRange Range, std::unique_ptr<Expr> E,
            std::unique_ptr<Misc> LParen, std::unique_ptr<Misc> RParen)
      : Expr(NK_ExprParen, Range), E(std::move(E)), LParen(std::move(LParen)),
        RParen(std::move(RParen)) {}

  [[nodiscard]] const Expr *expr() const { return E.get(); }
  [[nodiscard]] const Misc *lparen() const { return LParen.get(); }
  [[nodiscard]] const Misc *rparen() const { return RParen.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {E.get(), LParen.get(), RParen.get()};
  }
};

class ExprVar : public Expr {
  std::unique_ptr<Identifier> ID;

public:
  ExprVar(LexerCursorRange Range, std::unique_ptr<Identifier> ID)
      : Expr(NK_ExprVar, Range), ID(std::move(ID)) {
    assert(this->ID && "ID must not be null");
  }
  [[nodiscard]] const Identifier &id() const {
    assert(ID && "ID must not be null");
    return *ID;
  }

  [[nodiscard]] ChildVector children() const override { return {ID.get()}; }
};

} // namespace nixf
