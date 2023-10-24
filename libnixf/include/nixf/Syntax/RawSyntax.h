#pragma once

#include <memory>
#include <vector>

namespace nixf {

class Syntax;

enum class SyntaxKind {
  SK_Token,
  // expressions, the can be evaluated to "values"
  SK_BeginExpr,
#define EXPR(NAME) SK_##NAME,
#include "SyntaxKinds.inc"
#undef EXPR
  SK_EndExpr,

  SK_BeginNode,
#define NODE(NAME) SK_##NAME,
#include "SyntaxKinds.inc"
#undef NODE
  SK_EndNode,
};

/// Abstract class for syntax nodes.
/// Currently they are Twines and Tokens.
/// Tokens has trivial aand its content
struct RawNode {
protected:
  /// Text length.
  std::size_t Length;

public:
  /// Dump source code.
  virtual void dump(std::ostream &OS) const = 0;

  virtual ~RawNode() = default;

  [[nodiscard]] std::size_t getLength() const { return Length; }

  [[nodiscard]] virtual std::size_t getNumChildren() const { return 0; }

  /// \returns nth child, nullptr if we cannot find such index
  [[nodiscard]] virtual std::shared_ptr<RawNode>
  getNthChild(std::size_t N) const {
    return nullptr;
  }
};

/// Non-term constructs in a lanugage. They have children
class RawTwine : public RawNode {
  friend class SyntaxView;

  const std::vector<std::shared_ptr<RawNode>> Layout;

  SyntaxKind Kind;

public:
  RawTwine(SyntaxKind Kind, std::vector<std::shared_ptr<RawNode>> Layout);

  void dump(std::ostream &OS) const override;

  [[nodiscard]] std::size_t getNumChildren() const override {
    return Layout.size();
  }

  [[nodiscard]] std::shared_ptr<RawNode>
  getNthChild(std::size_t N) const override;
};

} // namespace nixf
