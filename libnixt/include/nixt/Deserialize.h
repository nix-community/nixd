#pragma once

#include "HookExpr.h"
#include "PtrPool.h"

#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>

#include <string_view>

namespace nixt {

/// \brief API Wrapper around nix::
///
/// Paths related to this context is very unstable.
struct DeserializeContext {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;

  /// Path resolution
  const nix::SourcePath BasePath;
  const nix::ref<nix::InputAccessor> RootFS;

  const nix::Pos::Origin &Origin;
};

/// \brief Stable API wrapper around official nix.
///
/// Because of "lazy-trees", these APIs have always breaked among nix updates,
/// the function wraps the context with a "stable" class `nix::EvalState`.
DeserializeContext getDeserializeContext(nix::EvalState &State,
                                         std::string_view BasePath,
                                         const nix::Pos::Origin &Origin);

nix::Expr *deserializeHookable(std::string_view &Data, DeserializeContext &Ctx,
                               PtrPool<nix::Expr> &Pool, ValueMap &VMap,
                               EnvMap &EMap);

} // namespace nixt
