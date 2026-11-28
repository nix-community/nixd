/// \file
/// \brief In-file declaration of the option set a document is written against.
///
/// A document may name the NixOS-style module that declares its options in its
/// leading comment block (so after a shebang or a license header too):
///
///     #:schema ./btc_line.module.nix
///
/// The spelling is taplo's, so that a generator emitting both a JSON Schema for
/// a TOML config and a module for a Nix config writes the same directive into
/// both.

#pragma once

#include "lspserver/Path.h"

#include <llvm/ADT/StringRef.h>

#include <optional>
#include <string>

namespace nixd {

/// \brief The path named by a `#:schema` directive in the leading comment
/// block of \p Src.
///
/// The returned reference points into \p Src.
std::optional<llvm::StringRef> parseSchemaDirective(std::string_view Src);

/// \brief Turn \p Directive into a module path to evaluate.
///
/// Resolution is relative to \p DocumentFile's own directory. The result must
/// stay within that directory or \p WorkspaceRoot: a document is opened, not
/// configured, so it may not point nixd at arbitrary files.
std::optional<std::string> resolveSchemaPath(llvm::StringRef Directive,
                                             lspserver::PathRef DocumentFile,
                                             llvm::StringRef WorkspaceRoot);

} // namespace nixd
