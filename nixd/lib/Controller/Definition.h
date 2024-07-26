#pragma once

#include "nixf/Sema/ParentMap.h"
#include "nixf/Sema/VariableLookup.h"

#include <llvm/Support/Error.h>

namespace nixd {

struct CannotFindVarException : std::exception {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot find variable on given node";
  }
};

/// \brief Heuristically find definition on some node
const nixf::Definition &findDefinition(const nixf::Node &N,
                                       const nixf::ParentMapAnalysis &PMA,
                                       const nixf::VariableLookupAnalysis &VLA);

} // namespace nixd
