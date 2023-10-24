#pragma once

#include "RawSyntax.h"

namespace nixf {

class SyntaxData {
  std::size_t Offset;
  std::shared_ptr<RawNode> Raw;
  const SyntaxData *Parent;

public:
  [[nodiscard]] std::shared_ptr<RawNode> getRaw() const { return Raw; }

  SyntaxData(std::size_t Offset, std::shared_ptr<RawNode> RN,
             const SyntaxData *Parent = nullptr);

  [[nodiscard]] std::size_t getNumChildren() const;

  /// \returns n-th child of this node, nullptr if we cannot find such index.
  [[nodiscard]] std::shared_ptr<SyntaxData> getNthChild(std::size_t N) const;

  /// nullable. If we are on root node.
  [[nodiscard]] const SyntaxData *getParent() const { return Parent; }

  [[nodiscard]] std::size_t getOffset() const { return Offset; }

  [[nodiscard]] SyntaxKind getSyntaxKind() const {
    return getRaw()->getSyntaxKind();
  }
};

} // namespace nixf
