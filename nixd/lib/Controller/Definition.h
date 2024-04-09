#pragma once

#include "nixf/Sema/ParentMap.h"
#include "nixf/Sema/VariableLookup.h"

#include <llvm/Support/Error.h>

namespace nixd {

/// \brief Heuristically find definition on some node
llvm::Expected<const nixf::Definition &>
findDefinition(const nixf::Node &N, const nixf::ParentMapAnalysis &PMA,
               const nixf::VariableLookupAnalysis &VLA);

} // namespace nixd
