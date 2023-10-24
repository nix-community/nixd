#pragma once

#include <memory>
#include <optional>

namespace nixf {

class SyntaxData;
class RawNode;

class Syntax {
  const std::shared_ptr<SyntaxData> Root;
  const SyntaxData *Data;

public:
  /// Root & Data must be non-null.
  Syntax(std::shared_ptr<SyntaxData> Root, const SyntaxData *Data);

  [[nodiscard]] std::optional<Syntax> getParent() const;

  [[nodiscard]] std::shared_ptr<RawNode> getRaw() const;
};

class ExprSyntax : Syntax {};

} // namespace nixf
