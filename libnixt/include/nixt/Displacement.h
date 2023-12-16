#pragma once

#include <nix/nixexpr.hh>

namespace nixt {

nix::PosIdx displOf(const nix::Expr *E, nix::Displacement Displ);

nix::PosIdx displOf(const nix::ExprAttrs *E, nix::Displacement Displ);

nix::PosIdx displOf(const nix::ExprLet *E, nix::Displacement Displ);

nix::PosIdx displOf(const nix::ExprLambda *E, nix::Displacement Displ);

} // namespace nixt
