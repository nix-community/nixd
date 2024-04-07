#include "nixt/Value.h"

#include <nix/attr-path.hh>

using namespace nixt;

bool nixt::isOption(nix::EvalState &State, nix::Value &V) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nAttrs)
    return false;

  // https://github.com/NixOS/nixpkgs/blob/58ca986543b591a8269cbce3328293ca8d64480f/lib/options.nix#L89
  try {
    auto S = attrPathStr(State, V, "_type");
    return S == "option";
  } catch (nix::AttrPathNotFound &Error) {
    return false;
  }
};

bool nixt::isDerivation(nix::EvalState &State, nix::Value &V) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nAttrs)
    return false;

  try {
    // Derivations has a special attribute "type" == "derivation"
    auto S = attrPathStr(State, V, "type");
    return S == "derivation";
  } catch (nix::AttrPathNotFound &Error) {
    return false;
  }
}

std::string nixt::attrPathStr(nix::EvalState &State, nix::Value &V,
                              const std::string &AttrPath) {
  auto &AutoArgs = *State.allocBindings(0);
  auto [VPath, Pos] = nix::findAlongAttrPath(State, AttrPath, AutoArgs, V);
  State.forceValue(*VPath, Pos);
  return std::string(State.forceStringNoCtx(*VPath, nix::noPos, ""));
}
