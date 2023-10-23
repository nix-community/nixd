#pragma once

#include <memory>
#include <vector>

namespace nixf {

enum class SyntaxKind { Token };

class SyntaxView;

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
};

/// Non-term constructs in a lanugage. They have children
class RawTwine {
  friend class SyntaxView;
  const SyntaxKind Kind;

  const std::vector<std::shared_ptr<RawNode>> Layout;

public:
  RawTwine(SyntaxKind Kind, std::vector<std::shared_ptr<RawNode>> Layout)
      : Kind(Kind), Layout(std::move(Layout)) {}
};

} // namespace nixf
