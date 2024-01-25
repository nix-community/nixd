/// \file
/// \brief AST nodes.
///
/// Declares the AST nodes used by the parser, the nodes may be used in latter
/// stages, for example semantic analysis.
/// It is expected that they may share some nodes, so they are reference
/// counted.

#pragma once

#include "nixf/Basic/Range.h"

#include <boost/container/small_vector.hpp>

#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace nixf {

class Node {
public:
  enum NodeKind {
#define NODE(NAME) NK_##NAME,
#include "nixf/Basic/NodeKinds.inc"
#undef NODE
    NK_BeginExpr,
#define EXPR(NAME) NK_##NAME,
#include "nixf/Basic/NodeKinds.inc"
#undef EXPR
    NK_EndExpr,
  };

private:
  NodeKind Kind;
  LexerCursorRange Range;

protected:
  explicit Node(NodeKind Kind, LexerCursorRange Range)
      : Kind(Kind), Range(Range) {}

public:
  [[nodiscard]] NodeKind kind() const { return Kind; }
  [[nodiscard]] LexerCursorRange range() const { return Range; }
  [[nodiscard]] PositionRange positionRange() const { return Range.range(); }
  [[nodiscard]] LexerCursor lCur() const { return Range.lCur(); }
  [[nodiscard]] LexerCursor rCur() const { return Range.rCur(); }
  [[nodiscard]] static const char *name(NodeKind Kind);
  [[nodiscard]] const char *name() const { return name(Kind); }

  using ChildVector = boost::container::small_vector<Node *, 8>;

  [[nodiscard]] virtual ChildVector children() const = 0;

  virtual ~Node() = default;

  /// \brief Descendant node that contains the given range.
  [[nodiscard]] const Node *descend(PositionRange Range) const {
    if (!positionRange().contains(Range)) {
      return nullptr;
    }
    for (const auto &Child : children()) {
      if (!Child)
        continue;
      if (Child->positionRange().contains(Range)) {
        return Child->descend(Range);
      }
    }
    return this;
  }
};

class Expr : public Node {
protected:
  explicit Expr(NodeKind Kind, LexerCursorRange Range) : Node(Kind, Range) {
    assert(NK_BeginExpr <= Kind && Kind <= NK_EndExpr);
  }
};

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

class InterpolablePart {
public:
  enum InterpolablePartKind {
    SPK_Escaped,
    SPK_Interpolation,
  };

private:
  InterpolablePartKind Kind;
  std::string Escaped;
  std::unique_ptr<Expr> Interpolation;

public:
  explicit InterpolablePart(std::string Escaped)
      : Kind(SPK_Escaped), Escaped(std::move(Escaped)), Interpolation(nullptr) {
  }

  explicit InterpolablePart(std::unique_ptr<Expr> Expr)
      : Kind(SPK_Interpolation), Interpolation(std::move(Expr)) {
    assert(this->Interpolation && "interpolation must not be null");
  }

  [[nodiscard]] InterpolablePartKind kind() const { return Kind; }

  [[nodiscard]] const std::string &escaped() const {
    assert(Kind == SPK_Escaped);
    return Escaped;
  }

  [[nodiscard]] const Expr &interpolation() const {
    assert(Kind == SPK_Interpolation);
    assert(Interpolation && "interpolation must not be null");
    return *Interpolation;
  }
};

class InterpolatedParts : public Node {
  std::vector<InterpolablePart> Fragments;

public:
  InterpolatedParts(LexerCursorRange Range,
                    std::vector<InterpolablePart> Fragments)
      : Node(NK_InterpolableParts, Range), Fragments(std::move(Fragments)) {}

  [[nodiscard]] const std::vector<InterpolablePart> &fragments() const {
    return Fragments;
  };

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

/// \brief Misc node, used for parentheses, keywords, etc.
///
/// This is used for representing nodes that only location matters.
/// Might be useful for linting.
class Misc : public Node {
public:
  Misc(LexerCursorRange Range) : Node(NK_Misc, Range) {}

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

/// \brief Identifier. Variable names, attribute names, etc.
class Identifier : public Node {
  std::string Name;

public:
  Identifier(LexerCursorRange Range, std::string Name)
      : Node(NK_Identifer, Range), Name(std::move(Name)) {}
  [[nodiscard]] const std::string &name() const { return Name; }

  [[nodiscard]] ChildVector children() const override { return {}; }
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

class AttrName : public Node {
public:
  enum AttrNameKind { ANK_ID, ANK_String, ANK_Interpolation };

private:
  AttrNameKind Kind;
  std::unique_ptr<Identifier> ID;
  std::unique_ptr<ExprString> String;
  std::unique_ptr<Expr> Interpolation;

public:
  [[nodiscard]] AttrNameKind kind() const { return Kind; }

  AttrName(std::unique_ptr<Identifier> ID, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_ID) {
    this->ID = std::move(ID);
    assert(this->ID && "ID must not be null");
  }

  AttrName(std::unique_ptr<ExprString> String, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_String) {
    this->String = std::move(String);
    assert(this->String && "String must not be null");
  }

  AttrName(std::unique_ptr<Expr> Interpolation, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_Interpolation) {
    this->Interpolation = std::move(Interpolation);
    assert(this->Interpolation && "Interpolation must not be null");
  }

  [[nodiscard]] const Expr &interpolation() const {
    assert(Kind == ANK_Interpolation);
    assert(Interpolation && "Interpolation must not be null");
    return *Interpolation;
  }

  [[nodiscard]] const Identifier &id() const {
    assert(Kind == ANK_ID);
    assert(ID && "ID must not be null");
    return *ID;
  }

  [[nodiscard]] const ExprString &string() const {
    assert(Kind == ANK_String);
    assert(String && "String must not be null");
    return *String;
  }

  [[nodiscard]] ChildVector children() const override {
    switch (Kind) {
    case ANK_ID:
      return {ID.get()};
    case ANK_String:
      return {String.get()};
    case ANK_Interpolation:
      return {Interpolation.get()};
    default:
      assert(false && "invalid AttrNameKind");
    }
    __builtin_unreachable();
  }
};

class AttrPath : public Node {
  std::vector<std::unique_ptr<AttrName>> Names;

public:
  AttrPath(LexerCursorRange Range, std::vector<std::unique_ptr<AttrName>> Names)
      : Node(NK_AttrPath, Range), Names(std::move(Names)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Names.size());
    for (const auto &Name : Names) {
      Children.push_back(Name.get());
    }
    return Children;
  }
};

class Binding : public Node {
  std::unique_ptr<AttrPath> Path;
  std::unique_ptr<Expr> Value;

public:
  Binding(LexerCursorRange Range, std::unique_ptr<AttrPath> Path,
          std::unique_ptr<Expr> Value)
      : Node(NK_Binding, Range), Path(std::move(Path)),
        Value(std::move(Value)) {
    assert(this->Path && "Path must not be null");
    // Value can be null, if missing in the syntax.
  }

  [[nodiscard]] const AttrPath &path() const {
    assert(Path && "Path must not be null");
    return *Path;
  }
  [[nodiscard]] const Expr *value() const { return Value.get(); }

  [[nodiscard]] ChildVector children() const override {
    return {Path.get(), Value.get()};
  }
};

class Inherit : public Node {
  std::vector<std::unique_ptr<AttrName>> Names;
  std::unique_ptr<Expr> E;

public:
  Inherit(LexerCursorRange Range, std::vector<std::unique_ptr<AttrName>> Names,
          std::unique_ptr<Expr> E)
      : Node(NK_Inherit, Range), Names(std::move(Names)), E(std::move(E)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<AttrName>> &names() const {
    return Names;
  }

  [[nodiscard]] bool hasExpr() { return E != nullptr; }

  [[nodiscard]] const Expr *expr() const { return E.get(); }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Names.size() + 1);
    for (const auto &Name : Names) {
      Children.push_back(Name.get());
    }
    Children.push_back(E.get());
    return Children;
  }
};

class Binds : public Node {
  std::vector<std::unique_ptr<Node>> Bindings;

public:
  Binds(LexerCursorRange Range, std::vector<std::unique_ptr<Node>> Bindings)
      : Node(NK_Binds, Range), Bindings(std::move(Bindings)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Node>> &bindings() const {
    return Bindings;
  }

  [[nodiscard]] ChildVector children() const override {
    ChildVector Children;
    Children.reserve(Bindings.size());
    for (const auto &Binding : Bindings) {
      Children.push_back(Binding.get());
    }
    return Children;
  }
};

class ExprAttrs : public Expr {
  std::unique_ptr<Binds> Body;
  std::unique_ptr<Misc> Rec;

public:
  ExprAttrs(LexerCursorRange Range, std::unique_ptr<Binds> Body,
            std::unique_ptr<Misc> Rec)
      : Expr(NK_ExprAttrs, Range), Body(std::move(Body)), Rec(std::move(Rec)) {}

  [[nodiscard]] const Binds *binds() const { return Body.get(); }
  [[nodiscard]] const Misc *rec() const { return Rec.get(); }

  [[nodiscard]] bool isRecursive() const { return Rec != nullptr; }

  [[nodiscard]] ChildVector children() const override {
    return {Body.get(), Rec.get()};
  }
};

} // namespace nixf
