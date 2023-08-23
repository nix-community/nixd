#pragma once

#include <nix/attr-path.hh>
#include <nix/eval.hh>
#include <nix/value.hh>

namespace nixd {

bool isOption(nix::EvalState &State, nix::Value &V);

bool isDerivation(nix::EvalState &State, nix::Value &V);

std::optional<std::string> attrPathStr(nix::EvalState &State, nix::Value &V,
                                       const std::string &AttrPath) noexcept;

/// Select the path given in \p AttrPath, and \return the value
nix::Value selectAttrPath(nix::EvalState &State, nix::Value Set,
                          const std::vector<std::string> &AttrPath);

extern int PrintDepth;

} // namespace nixd
