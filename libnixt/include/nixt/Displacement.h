/// \file
/// \brief Get `nix::PosIdx` of an `nix::Expr`, from `nix::Displacement`.
///
/// "Displacement" is something used in variable lookup.

#pragma once

#include <nix/expr/nixexpr.hh>

namespace nixt {

/// \brief Get `nix::PosIdx` of an `nix::Expr`, from `nix::Displacement`.
/// \note This is based on dynamic_cast, so it is not very efficient.
///
/// The function actually invokes `displOf()` of the corresponding `Expr` type.
nix::PosIdx displOf(const nix::Expr *E, nix::Displacement Displ);

/// \note The function asserts `E->recursive`. Since normal `ExprAttrs` cannot
/// do variable binding.
nix::PosIdx displOf(const nix::ExprAttrs *E, nix::Displacement Displ);

nix::PosIdx displOf(const nix::ExprLet *E, nix::Displacement Displ);

nix::PosIdx displOf(const nix::ExprLambda *E, nix::Displacement Displ);

} // namespace nixt
