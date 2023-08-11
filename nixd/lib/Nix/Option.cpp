#include "nixd/Nix/Option.h"
#include "nixd/Nix/EvalState.h"
#include "nixd/Nix/Value.h"

#include <nix/eval.hh>

#include <llvm/Support/FormatVariadic.h>

namespace nixd {

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

std::string OptionInfo::mdDoc() {
  return llvm::formatv(R"(
### Example
`{0}`
### Description
{1}
    )",
                       Example, Description);
}

} // namespace nixd
