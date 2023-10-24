#pragma once

#include "nixf/Syntax/RawSyntax.h"

#include <memory>
#include <optional>
#include <utility>

namespace nixf {

class SyntaxData;
class RawNode;

class Syntax {
protected:
  const std::shared_ptr<SyntaxData> Root;
  const SyntaxData *Data;

public:
  /// Root & Data must be non-null.
  Syntax(std::shared_ptr<SyntaxData> Root, const SyntaxData *Data);

  [[nodiscard]] std::optional<Syntax> getParent() const;

  [[nodiscard]] std::shared_ptr<RawNode> getRaw() const;

  [[nodiscard]] SyntaxKind getSyntaxKind() const;
};

class FormalSyntax : public Syntax {
  using Syntax::Syntax;

  [[nodiscard]] std::shared_ptr<SyntaxData> getName() const;
};

} // namespace nixf
