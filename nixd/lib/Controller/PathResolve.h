/// \file
/// \brief Shared path resolution utilities for Nix path literals.
///
/// This module provides unified path resolution logic used by both
/// DocumentLink and Go-to-Definition features.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace nixd {

/// \brief Resolve a Nix expression path to a real filesystem path.
///
/// This function resolves relative paths against the base file's directory,
/// handles directory → default.nix conversion, and validates the result.
///
/// Used by both DocumentLink (for clickable links) and Definition (for
/// go-to-definition on path literals).
///
/// \param BasePath The path of the file containing the path literal.
/// \param ExprPath The path literal string from the Nix expression.
/// \return The resolved absolute path, or nullopt if resolution fails.
inline std::optional<std::string> resolveExprPath(const std::string &BasePath,
                                                  const std::string &ExprPath) {
  namespace fs = std::filesystem;

  // Input validation: reject empty or excessively long paths
  if (ExprPath.empty() || ExprPath.length() > 4096)
    return std::nullopt;

  try {
    // Get the base directory from the file path
    std::error_code EC;
    fs::path BaseDir = fs::path(BasePath).parent_path();
    if (BaseDir.empty())
      return std::nullopt;

    // Canonicalize base directory to resolve symlinks
    fs::path BaseDirCanonical = fs::canonical(BaseDir, EC);
    if (EC)
      return std::nullopt;

    // Resolve the user-provided path relative to base directory
    // Use weakly_canonical to handle non-existent intermediate components
    fs::path ResolvedPath =
        fs::weakly_canonical(BaseDirCanonical / ExprPath, EC);
    if (EC)
      return std::nullopt;

    // Check if target exists
    auto Status = fs::status(ResolvedPath, EC);
    if (EC || !fs::exists(Status))
      return std::nullopt;

    // If it's a directory, append default.nix (Nix import convention)
    if (fs::is_directory(Status)) {
      ResolvedPath = ResolvedPath / "default.nix";
      if (!fs::exists(ResolvedPath, EC) || EC)
        return std::nullopt;
    }

    return ResolvedPath.string();
  } catch (const fs::filesystem_error &) {
    return std::nullopt;
  }
}

} // namespace nixd
