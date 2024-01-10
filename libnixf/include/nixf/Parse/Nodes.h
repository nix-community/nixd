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
    NK_BeginExpr,
    NK_StringParts,
    NK_ExprInt,
    NK_ExprFloat,
    NK_EndExpr,
  };

private:
  NodeKind Kind;
  OffsetRange Range;

protected:
  explicit Node(NodeKind Kind, OffsetRange Range) : Kind(Kind), Range(Range) {}

public:
  NodeKind getKind() { return Kind; }
  OffsetRange getRange() { return Range; }
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
  [[nodiscard]] NixInt getValue() const { return Value; }
};

class ExprFloat : public Expr {
  NixFloat Value;

public:
  ExprFloat(OffsetRange Range, NixFloat Value)
      : Expr(NK_ExprFloat, Range), Value(Value) {}
  [[nodiscard]] NixFloat getValue() const { return Value; }
};

class StringPart {
  enum StringPartKind {
    SPK_Escaped,
    SPK_Interpolation,
  } Kind;
  std::string Escaped;
  std::unique_ptr<Expr> Interpolation;

public:
  explicit StringPart(std::string Escaped)
      : Kind(SPK_Escaped), Escaped(std::move(Escaped)), Interpolation(nullptr) {
  }

  explicit StringPart(std::unique_ptr<Expr> Expr)
      : Kind(SPK_Interpolation), Interpolation(std::move(Expr)) {}

  StringPartKind getKind() { return Kind; }
};

class InterpolatedParts : public Expr {
  std::vector<StringPart> Fragments;

public:
  InterpolatedParts(OffsetRange Range, std::vector<StringPart> Fragments)
      : Expr(NK_StringParts, Range), Fragments(std::move(Fragments)) {}
};

} // namespace nixf
