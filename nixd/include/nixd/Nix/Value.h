#pragma once

#include <nix/attr-path.hh>
#include <nix/eval.hh>

namespace nixd {

bool isOption(nix::EvalState &State, nix::Value &V);

bool isDerivation(nix::EvalState &State, nix::Value &V);

std::optional<std::string> attrPathStr(nix::EvalState &State, nix::Value &V,
                                       const std::string &AttrPath) noexcept;

extern int PrintDepth;

} // namespace nixd
