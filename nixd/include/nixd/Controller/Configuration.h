/// \file
/// \brief Declares workspace configuration schema
#pragma once

#include <llvm/Support/JSON.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nixd {

// NOLINTBEGIN(readability-identifier-naming)
struct Configuration {
  struct Formatting {
    std::vector<std::string> command = {"nixfmt"};
  } formatting;

  struct OptionProvider {
    /// \brief Expression to eval. Select this attrset as eval .options
    std::string expr;
  };

  std::map<std::string, OptionProvider> options;

  struct NixpkgsProvider {
    /// \brief Expression to eval. Treat it as "import <nixpkgs> { }"
    std::string expr;
  } nixpkgs;

  struct Diagnostic {
    std::vector<std::string> suppress;
  } diagnostic;
};

/// \brief A partial configuration supplied by one configuration source.
struct ConfigurationPatch {
  struct Formatting {
    std::optional<std::vector<std::string>> command;
  };

  struct NixpkgsProvider {
    std::optional<std::string> expr;
  };

  struct Diagnostic {
    std::optional<std::vector<std::string>> suppress;
  };

  std::optional<Formatting> formatting;
  std::optional<std::map<std::string, Configuration::OptionProvider>> options;
  std::optional<NixpkgsProvider> nixpkgs;
  std::optional<Diagnostic> diagnostic;
};

/// \brief The configuration used when no external source overrides a field.
Configuration defaultConfiguration();

/// \brief Apply a partial configuration over an existing configuration.
Configuration overlay(Configuration Base, const ConfigurationPatch &Patch);

bool fromJSON(const llvm::json::Value &Params,
              Configuration::OptionProvider &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params,
              ConfigurationPatch::Formatting &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params,
              ConfigurationPatch::NixpkgsProvider &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params,
              ConfigurationPatch::Diagnostic &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, ConfigurationPatch &R,
              llvm::json::Path P);

// NOLINTEND(readability-identifier-naming)

} // namespace nixd
