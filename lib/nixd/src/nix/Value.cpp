#include "nixd/nix/Value.h"

namespace nix::nixd {

bool isOption(EvalState &State, Value &V) {
  State.forceValue(V, noPos);
  if (V.type() != ValueType::nAttrs)
    return false;
  bool HasDesc = false;
  bool HasType = false;
  for (auto Attr : *V.attrs) {
    auto Name = State.symbols[Attr.name];
    if (std::string_view(Name) == "description")
      HasDesc = true;
    if (std::string_view(Name) == "type")
      HasType = true;
  }
  return HasType && HasDesc;
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
