#pragma once

#include <nix/eval.hh>

#include "llvm/Support/FormatVariadic.h"

#include <optional>
#include <string>

namespace nix::nixd {
struct OptionInfo {
  std::optional<std::string> Type;
  std::optional<std::string> Description;
  std::optional<std::string> Example;

  std::string mdDoc() {
    return llvm::formatv(R"(
### Example
`{0}`
### Description
{1}
    )",
                         Example, Description);
  }
};

OptionInfo optionInfo(nix::EvalState &State, nix::Value &V);
} // namespace nix::nixd
