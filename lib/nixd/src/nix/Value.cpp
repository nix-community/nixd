#include "nixd/nix/Value.h"

namespace nix::nixd {

bool isOption(EvalState &State, Value &V) {
  State.forceValue(V, noPos);
  if (V.type() != ValueType::nAttrs)
    return false;

  // https://github.com/NixOS/nixpkgs/blob/58ca986543b591a8269cbce3328293ca8d64480f/lib/options.nix#L89
  auto S = attrPathStr(State, V, "_type");
  return S && S.value() == "option";
};

std::optional<std::string> attrPathStr(nix::EvalState &State, nix::Value &V,
                                       const std::string &AttrPath) noexcept {
  try {
    auto [VPath, _] =
        findAlongAttrPath(State, AttrPath, *State.allocBindings(0), V);
    if (VPath->type() == nix::ValueType::nString)
      return std::string(State.forceStringNoCtx(*VPath, nix::noPos, ""));
  } catch (std::exception &E) {
  } catch (...) {
  }
  return std::nullopt;
};

} // namespace nix::nixd
