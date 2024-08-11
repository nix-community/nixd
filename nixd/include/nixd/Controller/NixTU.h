#pragma once

#include "nixd/Support/OwnedRegion.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Basic.h"
#include "nixf/Sema/ParentMap.h"
#include "nixf/Sema/VariableLookup.h"

#include <memory>
#include <optional>
#include <vector>

namespace nixd {

/// \brief Holds analyzed information about a document.
///
/// TU stands for "Translation Unit".
class NixTU {
  std::vector<nixf::Diagnostic> Diagnostics;
  std::shared_ptr<nixf::Node> AST;
  std::optional<util::OwnedRegion> ASTByteCode;
  std::unique_ptr<nixf::VariableLookupAnalysis> VLA;
  std::unique_ptr<nixf::ParentMapAnalysis> PMA;
  std::shared_ptr<const std::string> Src;

public:
  NixTU() = default;
  NixTU(std::vector<nixf::Diagnostic> Diagnostics,
        std::shared_ptr<nixf::Node> AST,
        std::optional<util::OwnedRegion> ASTByteCode,
        std::unique_ptr<nixf::VariableLookupAnalysis> VLA,
        std::shared_ptr<const std::string> Src);

  [[nodiscard]] const std::vector<nixf::Diagnostic> &diagnostics() const {
    return Diagnostics;
  }

  [[nodiscard]] const std::shared_ptr<nixf::Node> &ast() const { return AST; }

  [[nodiscard]] const nixf::ParentMapAnalysis *parentMap() const {
    return PMA.get();
  }

  [[nodiscard]] const nixf::VariableLookupAnalysis *variableLookup() const {
    return VLA.get();
  }

  [[nodiscard]] std::string_view src() const { return *Src; }
};

} // namespace nixd
