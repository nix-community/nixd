/// \file
/// \brief Construct child -> parent relations of `nix::Expr` nodes.

#pragma once

#include "Visitor.h"

namespace nixt {

/// \brief The parent map. The key is "child", the value is "parent".
using ParentMap = std::map<const nix::Expr *, const nix::Expr *>;

/// \brief Construct child -> parent relations of `nix::Expr` nodes.
ParentMap parentMap(const nix::Expr *Root);

} // namespace nixt
