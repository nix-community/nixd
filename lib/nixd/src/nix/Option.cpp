#include "nixd/nix/Option.h"
#include "nixd/nix/EvalState.h"
#include "nixd/nix/Value.h"

#include <nix/eval.hh>

namespace nix::nixd {

OptionInfo optionInfo(nix::EvalState &State, nix::Value &V) {
  OptionInfo Info;
  Info.Type = attrPathStr(State, V, "type.name");
  Info.Description = attrPathStr(State, V, "description");
  if (!Info.Description.has_value())
    Info.Description = attrPathStr(State, V, "description.text");
  try {
    auto [VExample, _] =
        findAlongAttrPath(State, "example", *State.allocBindings(0), V);

    // Use limitted depth, to avoid infinite recursion
    forceValueDepth(State, V, 10);
    // If the type is "literal expression"
    if (auto VText = attrPathStr(State, V, "example.text"))
      Info.Example = VText;
    else
      Info.Example = printValue(State, *VExample);
  } catch (...) {
  }

  return Info;
};

} // namespace nix::nixd
