/// \file
/// \brief Shared path resolution utilities for Nix path literals.
///
/// This module provides unified path resolution logic used by both
/// DocumentLink and Go-to-Definition features.

#pragma once

#include "lspserver/Logger.h"

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

  lspserver::vlog("path-resolve: BasePath={0}, ExprPath={1}", BasePath,
                  ExprPath);

  // Input validation: reject empty or excessively long paths
  if (ExprPath.empty() || ExprPath.length() > 4096) {
    lspserver::elog("path-resolve: input validation failed (empty={0}, "
                    "length={1})",
                    ExprPath.empty(), ExprPath.length());
    return std::nullopt;
  }

  try {
    // Get the base directory from the file path
    std::error_code EC;
    fs::path BaseDir = fs::path(BasePath).parent_path();
    if (BaseDir.empty()) {
      lspserver::elog("path-resolve: base directory is empty");
      return std::nullopt;
    }

    // Canonicalize base directory to resolve symlinks
    fs::path BaseDirCanonical = fs::canonical(BaseDir, EC);
    if (EC) {
      lspserver::elog("path-resolve: canonical failed for {0}: {1}",
                      BaseDir.string(), EC.message());
      return std::nullopt;
    }

    // Resolve the user-provided path relative to base directory
    // Use weakly_canonical to handle non-existent intermediate components
    fs::path ResolvedPath =
        fs::weakly_canonical(BaseDirCanonical / ExprPath, EC);
    if (EC) {
      lspserver::elog("path-resolve: weakly_canonical failed for {0}: {1}",
                      (BaseDirCanonical / ExprPath).string(), EC.message());
      return std::nullopt;
    }

    // Check if target exists
    auto Status = fs::status(ResolvedPath, EC);
    if (EC || !fs::exists(Status)) {
      lspserver::elog("path-resolve: target does not exist: {0}",
                      ResolvedPath.string());
      return std::nullopt;
    }

    // If it's a directory, append default.nix (Nix import convention)
    if (fs::is_directory(Status)) {
      ResolvedPath = ResolvedPath / "default.nix";
      if (!fs::exists(ResolvedPath, EC) || EC) {
        lspserver::elog("path-resolve: default.nix not found in directory: {0}",
                        ResolvedPath.string());
        return std::nullopt;
      }
    }

    lspserver::vlog("path-resolve: resolved to {0}", ResolvedPath.string());
    return ResolvedPath.string();
  } catch (const fs::filesystem_error &E) {
    lspserver::elog("path-resolve: filesystem error: {0}", E.what());
    return std::nullopt;
  }
}

} // namespace nixd
