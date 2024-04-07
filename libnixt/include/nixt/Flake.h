/// \file
/// \brief Utilities about nix flake.

#pragma once

#include <nix/eval.hh>

#include <string_view>

namespace nixt {

/// Call nix flake, but do not use any "fetchers".
void callDirtyFlake(nix::EvalState &State, std::string_view SrcPath,
                    nix::Value &VRes);

} // namespace nixt
