#pragma once

#include <nix/eval.hh>

#include <optional>
#include <string>

namespace nix::nixd {
struct OptionInfo {
  std::optional<std::string> Type;
  std::optional<std::string> Description;
};

OptionInfo optionInfo(nix::EvalState &State, nix::Value &V);
} // namespace nix::nixd
