#pragma once

#include "nixd/util/OwnedRegion.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Basic.h"

namespace nixd {

/// \brief Holds analyzed information about a document.
///
/// TU stands for "Translation Unit".
class NixTU {
  std::vector<nixf::Diagnostic> Diagnostics;
  std::shared_ptr<nixf::Node> AST;
  std::optional<util::OwnedRegion> ASTByteCode;

public:
  NixTU() = default;
  NixTU(std::vector<nixf::Diagnostic> Diagnostics,
        std::shared_ptr<nixf::Node> AST,
        std::optional<util::OwnedRegion> ASTByteCode)
      : Diagnostics(std::move(Diagnostics)), AST(std::move(AST)),
        ASTByteCode(std::move(ASTByteCode)) {}

  [[nodiscard]] const std::vector<nixf::Diagnostic> &diagnostics() const {
    return Diagnostics;
  }

  [[nodiscard]] const std::shared_ptr<nixf::Node> &ast() const { return AST; }
};

} // namespace nixd
