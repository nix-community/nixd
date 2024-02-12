#pragma once

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes.h"

namespace nixd {

/// \brief Holds analyzed information about a document.
///
/// TU stands for "Translation Unit".
class NixTU {
  std::vector<nixf::Diagnostic> Diagnostics;
  std::unique_ptr<nixf::Node> AST;

public:
  NixTU() = default;
  NixTU(std::vector<nixf::Diagnostic> Diagnostics,
        std::unique_ptr<nixf::Node> AST)
      : Diagnostics(std::move(Diagnostics)), AST(std::move(AST)) {}

  [[nodiscard]] const std::vector<nixf::Diagnostic> &diagnostics() const {
    return Diagnostics;
  }

  [[nodiscard]] const std::unique_ptr<nixf::Node> &ast() const { return AST; }
};

} // namespace nixd
