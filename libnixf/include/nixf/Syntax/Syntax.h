#pragma once

#include <memory>
#include <optional>

namespace nixf {

class SyntaxData;

class Syntax {
  const std::shared_ptr<SyntaxData> Root;
  const SyntaxData *Data;

public:
  /// Root & Data must be non-null.
  Syntax(std::shared_ptr<SyntaxData> Root, const SyntaxData *Data);

  std::optional<Syntax> getParent();
};

} // namespace nixf
