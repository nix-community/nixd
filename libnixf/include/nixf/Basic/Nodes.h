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
    NK_InterpolableParts,

    /// \brief Misc node, used for parentheses, keywords, etc.
    /// \see Misc
    NK_Misc,

    NK_Identifer,
    NK_AttrName,
    NK_AttrPath,
    NK_Binding,
    NK_Binds,

    NK_BeginExpr,
    NK_ExprInt,
    NK_ExprFloat,
    NK_ExprVar,
    NK_ExprString,
    NK_ExprPath,
    NK_ExprParen,
    NK_ExprAttrs,
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
  [[nodiscard]] PositionRange positionRange() const {
    return Range.positionRange();
  }
  [[nodiscard]] LexerCursor begin() const { return Range.begin(); }
  [[nodiscard]] LexerCursor end() const { return Range.end(); }
  [[nodiscard]] static const char *name(NodeKind Kind);
  [[nodiscard]] const char *name() const { return name(Kind); }

  using ChildVector = boost::container::small_vector<Node *, 8>;

  [[nodiscard]] virtual ChildVector children() const = 0;

  /// \brief Descendant node that contains the given range.
  [[nodiscard]] const Node *descend(PositionRange Range) const {
    if (!positionRange().contains(Range)) {
      return nullptr;
    }
    for (const auto &Child : children()) {
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
  std::shared_ptr<Expr> Interpolation;

public:
  explicit InterpolablePart(std::string Escaped)
      : Kind(SPK_Escaped), Escaped(std::move(Escaped)), Interpolation(nullptr) {
  }

  explicit InterpolablePart(std::shared_ptr<Expr> Expr)
      : Kind(SPK_Interpolation), Interpolation(std::move(Expr)) {}

  [[nodiscard]] InterpolablePartKind kind() const { return Kind; }

  [[nodiscard]] const std::string &escaped() const {
    assert(Kind == SPK_Escaped);
    return Escaped;
  }

  [[nodiscard]] const std::shared_ptr<Expr> &interpolation() const {
    assert(Kind == SPK_Interpolation);
    return Interpolation;
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
  std::shared_ptr<InterpolatedParts> Parts;

public:
  ExprString(LexerCursorRange Range, std::shared_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprString, Range), Parts(std::move(Parts)) {}

  [[nodiscard]] const std::shared_ptr<InterpolatedParts> &parts() const {
    return Parts;
  }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

class ExprPath : public Expr {
  std::shared_ptr<InterpolatedParts> Parts;

public:
  ExprPath(LexerCursorRange Range, std::shared_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprPath, Range), Parts(std::move(Parts)) {}

  [[nodiscard]] const std::shared_ptr<InterpolatedParts> &parts() const {
    return Parts;
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
  std::shared_ptr<Expr> E;
  std::shared_ptr<Misc> LParen;
  std::shared_ptr<Misc> RParen;

public:
  ExprParen(LexerCursorRange Range, std::shared_ptr<Expr> E,
            std::shared_ptr<Misc> LParen, std::shared_ptr<Misc> RParen)
      : Expr(NK_ExprParen, Range), E(std::move(E)), LParen(std::move(LParen)),
        RParen(std::move(RParen)) {}

  [[nodiscard]] const std::shared_ptr<Expr> &expr() const { return E; }
  [[nodiscard]] const std::shared_ptr<Misc> &lparen() const { return LParen; }
  [[nodiscard]] const std::shared_ptr<Misc> &rparen() const { return RParen; }

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
  std::shared_ptr<Identifier> ID;

public:
  ExprVar(LexerCursorRange Range, std::shared_ptr<Identifier> ID)
      : Expr(NK_ExprVar, Range), ID(std::move(ID)) {}
  [[nodiscard]] const std::shared_ptr<Identifier> &id() const { return ID; }

  [[nodiscard]] ChildVector children() const override { return {ID.get()}; }
};

class AttrName : public Node {
public:
  enum AttrNameKind { ANK_ID, ANK_String, ANK_Interpolation };

private:
  AttrNameKind Kind;
  std::shared_ptr<Identifier> ID;
  std::shared_ptr<ExprString> String;
  std::shared_ptr<Expr> Interpolation;

public:
  [[nodiscard]] AttrNameKind kind() const { return Kind; }

  AttrName(std::shared_ptr<Identifier> ID, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_ID) {
    this->ID = std::move(ID);
  }

  AttrName(std::shared_ptr<ExprString> String, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_String) {
    this->String = std::move(String);
  }

  AttrName(std::shared_ptr<Expr> Interpolation, LexerCursorRange Range)
      : Node(NK_AttrName, Range), Kind(ANK_Interpolation) {
    this->Interpolation = std::move(Interpolation);
  }

  [[nodiscard]] const std::shared_ptr<Expr> &interpolation() const {
    assert(Kind == ANK_Interpolation);
    return Interpolation;
  }

  [[nodiscard]] const std::shared_ptr<Identifier> &id() const {
    assert(Kind == ANK_ID);
    return ID;
  }

  [[nodiscard]] const std::shared_ptr<ExprString> &string() const {
    assert(Kind == ANK_String);
    return String;
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
  }
};

class AttrPath : public Node {
  std::vector<std::shared_ptr<AttrName>> Names;

public:
  AttrPath(LexerCursorRange Range, std::vector<std::shared_ptr<AttrName>> Names)
      : Node(NK_AttrPath, Range), Names(std::move(Names)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<AttrName>> &names() const {
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
  std::shared_ptr<AttrPath> Path;
  std::shared_ptr<Expr> Value;

public:
  Binding(LexerCursorRange Range, std::shared_ptr<AttrPath> Path,
          std::shared_ptr<Expr> Value)
      : Node(NK_Binding, Range), Path(std::move(Path)),
        Value(std::move(Value)) {}

  [[nodiscard]] const std::shared_ptr<AttrPath> &path() const { return Path; }
  [[nodiscard]] const std::shared_ptr<Expr> &value() const { return Value; }

  [[nodiscard]] ChildVector children() const override {
    return {Path.get(), Value.get()};
  }
};

class Binds : public Node {
  std::vector<std::shared_ptr<Node>> Bindings;

public:
  Binds(LexerCursorRange Range, std::vector<std::shared_ptr<Node>> Bindings)
      : Node(NK_Binds, Range), Bindings(std::move(Bindings)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<Node>> &bindings() const {
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
  std::shared_ptr<Binds> Body;
  std::shared_ptr<Misc> Rec;

public:
  ExprAttrs(LexerCursorRange Range, std::shared_ptr<Binds> Body,
            std::shared_ptr<Misc> Rec)
      : Expr(NK_ExprAttrs, Range), Body(std::move(Body)), Rec(std::move(Rec)) {}

  [[nodiscard]] const std::shared_ptr<Binds> &binds() const { return Body; }
  [[nodiscard]] const std::shared_ptr<Misc> &rec() const { return Rec; }

  [[nodiscard]] bool isRecursive() const { return Rec != nullptr; }

  [[nodiscard]] ChildVector children() const override {
    return {Body.get(), Rec.get()};
  }
};

} // namespace nixf
