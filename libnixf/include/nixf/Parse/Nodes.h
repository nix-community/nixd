/// \file
/// \brief AST nodes.
///
/// Declares the AST nodes used by the parser, the nodes may be used in latter
/// stages, for example semantic analysis.
/// It is expected that they may share some nodes, so they are reference
/// counted.

#pragma once

#include "nixf/Basic/Range.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace nixf {

class Node {
public:
  enum NodeKind {
    NK_InterpolableParts,
    NK_BeginExpr,
    NK_ExprInt,
    NK_ExprFloat,
    NK_ExprString,
    NK_ExprPath,
    NK_EndExpr,
  };

private:
  NodeKind Kind;
  OffsetRange Range;

protected:
  explicit Node(NodeKind Kind, OffsetRange Range) : Kind(Kind), Range(Range) {}

public:
  [[nodiscard]] NodeKind kind() const { return Kind; }
  [[nodiscard]] OffsetRange range() const { return Range; }
  [[nodiscard]] const char *begin() const { return Range.Begin; }
  [[nodiscard]] const char *end() const { return Range.End; }
};

class Expr : public Node {
protected:
  explicit Expr(NodeKind Kind, OffsetRange Range) : Node(Kind, Range) {
    assert(NK_BeginExpr <= Kind && Kind <= NK_EndExpr);
  }
};

using NixInt = int64_t;
using NixFloat = double;

class ExprInt : public Expr {
  NixInt Value;

public:
  ExprInt(OffsetRange Range, NixInt Value)
      : Expr(NK_ExprInt, Range), Value(Value) {}
  [[nodiscard]] NixInt value() const { return Value; }
};

class ExprFloat : public Expr {
  NixFloat Value;

public:
  ExprFloat(OffsetRange Range, NixFloat Value)
      : Expr(NK_ExprFloat, Range), Value(Value) {}
  [[nodiscard]] NixFloat value() const { return Value; }
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
};

class InterpolatedParts : public Node {
  std::vector<InterpolablePart> Fragments;

public:
  InterpolatedParts(OffsetRange Range, std::vector<InterpolablePart> Fragments)
      : Node(NK_InterpolableParts, Range), Fragments(std::move(Fragments)) {}

  [[nodiscard]] const std::vector<InterpolablePart> &fragments() const {
    return Fragments;
  };
};

class ExprString : public Expr {
  std::shared_ptr<InterpolatedParts> Parts;

public:
  ExprString(OffsetRange Range, std::shared_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprString, Range), Parts(std::move(Parts)) {}

  [[nodiscard]] const std::shared_ptr<InterpolatedParts> &parts() const {
    return Parts;
  }
};

class ExprPath : public Expr {
  std::shared_ptr<InterpolatedParts> Parts;

public:
  ExprPath(OffsetRange Range, std::shared_ptr<InterpolatedParts> Parts)
      : Expr(NK_ExprPath, Range), Parts(std::move(Parts)) {}

  [[nodiscard]] const std::shared_ptr<InterpolatedParts> &parts() const {
    return Parts;
  }
};

} // namespace nixf
