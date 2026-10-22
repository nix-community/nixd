#pragma once

#include "nixf/Basic/Nodes/Comment.h"
#include "nixf/Basic/Range.h"

#include <boost/container/small_vector.hpp>

#include <cassert>
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
  std::vector<CommentPtr> LeadingComments;
  std::vector<CommentPtr> TrailingComments;

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

  [[nodiscard]] std::string_view src(std::string_view Src) const {
    auto Begin = lCur().offset();
    auto Length = rCur().offset() - Begin;
    return Src.substr(Begin, Length);
  }

  void addLeadingComment(CommentPtr Comment) {
    LeadingComments.push_back(std::move(Comment));
  }

  void addTrailingComment(CommentPtr Comment) {
    TrailingComments.push_back(std::move(Comment));
  }

  [[nodiscard]] const std::vector<CommentPtr> &leadingComments() const {
    return LeadingComments;
  }

  [[nodiscard]] const std::vector<CommentPtr> &trailingComments() const {
    return TrailingComments;
  }
};

class Expr : public Node {
protected:
  explicit Expr(NodeKind Kind, LexerCursorRange Range) : Node(Kind, Range) {
    assert(NK_BeginExpr <= Kind && Kind <= NK_EndExpr);
  }

public:
  static bool classof(const Node *N) { return isExpr(N->kind()); }

  static bool isExpr(NodeKind Kind) {
    return NK_BeginExpr <= Kind && Kind <= NK_EndExpr;
  }

  /// \returns true if the expression might be evaluated to lambda.
  static bool maybeLambda(NodeKind Kind) {
    if (!isExpr(Kind))
      return false;
    switch (Kind) {
    case Node::NK_ExprInt:
    case Node::NK_ExprFloat:
    case Node::NK_ExprAttrs:
    case Node::NK_ExprString:
    case Node::NK_ExprPath:
      return false;
    default:
      return true;
    }
  }

  [[nodiscard]] bool maybeLambda() const { return maybeLambda(kind()); }
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

/// \brief Identifier. Variable names, attribute names, etc.
class Identifier : public Node {
  const std::string Name;

public:
  Identifier(LexerCursorRange Range, std::string Name)
      : Node(NK_Identifier, Range), Name(std::move(Name)) {}
  [[nodiscard]] const std::string &name() const { return Name; }

  [[nodiscard]] ChildVector children() const override { return {}; }
};

/// \brief Holds a "." in the language.
class Dot : public Node {
  const Node *Prev;
  const Node *Next;

public:
  Dot(LexerCursorRange Range, const Node *Prev, const Node *Next)
      : Node(NK_Dot, Range), Prev(Prev), Next(Next) {
    assert(Prev);
  }

  [[nodiscard]] ChildVector children() const override { return {}; }

  [[nodiscard]] const Node &prev() const {
    assert(Prev);
    return *Prev;
  }
  [[nodiscard]] const Node *next() const { return Next; }
};

} // namespace nixf
