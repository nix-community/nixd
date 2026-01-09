/// \file
/// \brief Shared utilities for code actions.
///
/// This header provides common functions used across multiple code action
/// implementations, including text manipulation, Nix identifier validation,
/// and CodeAction construction helpers.

#pragma once

#include <lspserver/Protocol.h>

#include <llvm/ADT/StringRef.h>

#include <string>
#include <vector>

namespace nixd {

/// \brief Create a CodeAction with a single text edit.
lspserver::CodeAction createSingleEditAction(const std::string &Title,
                                             llvm::StringLiteral Kind,
                                             const std::string &FileURI,
                                             const lspserver::Range &EditRange,
                                             std::string NewText);

/// \brief Check if a string is a valid Nix identifier that can be unquoted.
///
/// Valid identifiers: start with letter or underscore, contain letters,
/// digits, underscores, hyphens, or single quotes. Must not be a keyword.
bool isValidNixIdentifier(const std::string &S);

/// \brief Escape special characters for Nix double-quoted string literals.
///
/// Escapes: " \ ${ \n \r \t (per Nix Reference Manual)
std::string escapeNixString(llvm::StringRef S);

/// \brief Quote and escape a Nix attribute key if necessary.
///
/// Returns the key as-is if it's a valid identifier, otherwise quotes and
/// escapes special characters using escapeNixString().
std::string quoteNixAttrKey(const std::string &Key);

/// \brief Maximum recursion depth for JSON to Nix conversion.
constexpr size_t MaxJsonDepth = 100;

/// \brief Maximum array/object width for JSON to Nix conversion.
constexpr size_t MaxJsonWidth = 10000;

} // namespace nixd
