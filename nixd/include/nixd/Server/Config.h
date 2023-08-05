#pragma once

#include <list>
#include <string>
#include <thread>
#include <vector>

namespace nixd::configuration {

inline std::list<std::string> toNixArgs(const std::vector<std::string> &Args) {
  return {Args.begin(), Args.end()};
}
struct InstallableConfigurationItem {
  std::vector<std::string> args;
  std::string installable;

  [[nodiscard]] bool empty() const {
    return args.empty() && installable.empty();
  }

  [[nodiscard]] auto nArgs() const { return toNixArgs(args); }
};

struct TopLevel {

  struct Eval {
    /// Nix installables that will be used for root translation unit.
    InstallableConfigurationItem target;

    /// The depth you'd like to eval *after* reached "installable" target.
    int depth = 0;
    /// Number of workers forking
    /// defaults to std::thread::hardware_concurrency
    int workers = static_cast<int>(std::thread::hardware_concurrency());
  };

  Eval eval;

  struct Formatting {
    std::string command = "nixpkgs-fmt";
  };
  Formatting formatting;

  struct Options {
    bool enable = false;
    InstallableConfigurationItem target;
  };

  Options options;
};

} // namespace nixd::configuration
