#pragma once

#include <nix/attr-path.hh>
#include <nix/eval.hh>

namespace nix::nixd {

bool isOption(EvalState &State, Value &V);

bool isDerivation(EvalState &State, Value &V);

std::optional<std::string> attrPathStr(nix::EvalState &State, nix::Value &V,
                                       const std::string &AttrPath) noexcept;

} // namespace nix::nixd
