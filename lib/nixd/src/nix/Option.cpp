#include "nixd/nix/Option.h"
#include "nixd/nix/Value.h"

namespace nix::nixd {

OptionInfo optionInfo(nix::EvalState &State, nix::Value &V) {
  OptionInfo Info;
  Info.Type = attrPathStr(State, V, "type.name");
  Info.Description = attrPathStr(State, V, "description");
  if (!Info.Description.has_value())
    Info.Description = attrPathStr(State, V, "description.text");

  return Info;
};

} // namespace nix::nixd
