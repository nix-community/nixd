#pragma once

#include <nix/nixexpr.hh>

namespace nixt {

// Get printable name of some expression
const char *nameOf(const nix::Expr *E);

} // namespace nixt
