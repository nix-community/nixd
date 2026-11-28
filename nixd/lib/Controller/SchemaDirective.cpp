#include "SchemaDirective.h"
#include "PathResolve.h"

#include "lspserver/Logger.h"

#include <llvm/ADT/StringExtras.h>

#include <filesystem>

using namespace lspserver;
using namespace nixd;

namespace {

namespace fs = std::filesystem;

/// Both arguments must already be canonical.
bool contains(const fs::path &Dir, const fs::path &P) {
  if (Dir.empty())
    return false;
  const fs::path Rel = P.lexically_relative(Dir);
  return !Rel.empty() && *Rel.begin() != "..";
}

/// The resolved path is spliced into a Nix expression as a path literal, so
/// anything outside the literal's grammar has to be turned away here rather
/// than produce a syntax error -- or an injection -- at eval time.
bool isNixPathLiteral(llvm::StringRef P) {
  return P.starts_with("/") && llvm::all_of(P, [](char C) {
           return llvm::isAlnum(C) || llvm::StringRef("._-+/").contains(C);
         });
}

std::optional<fs::path> canonicalOrNone(const fs::path &P) {
  std::error_code EC;
  fs::path C = fs::canonical(P, EC);
  if (EC)
    return std::nullopt;
  return C;
}

} // namespace

std::optional<llvm::StringRef>
nixd::parseSchemaDirective(std::string_view Src) {
  llvm::StringRef Rest(Src.data(), Src.size());
  while (!Rest.empty()) {
    llvm::StringRef Line;
    std::tie(Line, Rest) = Rest.split('\n');
    Line = Line.rtrim();
    if (Line.empty())
      continue;
    if (!Line.starts_with("#"))
      return std::nullopt;
    if (!Line.consume_front("#:schema"))
      continue;
    llvm::StringRef Path = Line.ltrim();
    // Reject `#:schemafoo`: the keyword must be followed by whitespace.
    if (Path.empty() || Path.size() == Line.size())
      return std::nullopt;
    return Path;
  }
  return std::nullopt;
}

std::optional<std::string>
nixd::resolveSchemaPath(llvm::StringRef Directive, PathRef DocumentFile,
                        llvm::StringRef WorkspaceRoot) {
  std::optional<std::string> Resolved =
      resolveExprPath(DocumentFile.str(), Directive.str());
  if (!Resolved) {
    elog("schema-directive: cannot resolve {0} from {1}", Directive,
         DocumentFile);
    return std::nullopt;
  }

  const std::optional<fs::path> DocumentDir =
      canonicalOrNone(fs::path(DocumentFile.str()).parent_path());
  const std::optional<fs::path> Root =
      WorkspaceRoot.empty() ? std::nullopt
                            : canonicalOrNone(fs::path(WorkspaceRoot.str()));
  const fs::path Target(*Resolved);
  if (!(DocumentDir && contains(*DocumentDir, Target)) &&
      !(Root && contains(*Root, Target))) {
    elog("schema-directive: {0} resolves to {1}, outside both the document's "
         "directory and the workspace root; ignoring",
         Directive, *Resolved);
    return std::nullopt;
  }

  if (!isNixPathLiteral(*Resolved)) {
    elog("schema-directive: {0} resolves to {1}, which is not spellable as a "
         "Nix path literal; ignoring",
         Directive, *Resolved);
    return std::nullopt;
  }

  return Resolved;
}
