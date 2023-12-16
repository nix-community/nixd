#pragma once

#include "Visitor.h"

namespace nixt {

using ParentMap = std::map<const nix::Expr *, const nix::Expr *>;

ParentMap parentMap(const nix::Expr *Root);

} // namespace nixt
