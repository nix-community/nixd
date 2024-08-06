/// \file
/// \brief Declares workspace configuration schema
#pragma once

#include <llvm/Support/JSON.h>

#include <map>
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

bool fromJSON(const llvm::json::Value &Params, Configuration::Diagnostic &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Configuration::Formatting &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Configuration::OptionProvider &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params,
              Configuration::NixpkgsProvider &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Configuration &R,
              llvm::json::Path P);

// NOLINTEND(readability-identifier-naming)

} // namespace nixd
