/// \file
/// \brief Implementation of shared utilities for code actions.

#include "Utils.h"

#include <set>

namespace nixd {

lspserver::CodeAction createSingleEditAction(const std::string &Title,
                                             llvm::StringLiteral Kind,
                                             const std::string &FileURI,
                                             const lspserver::Range &EditRange,
                                             std::string NewText) {
  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(
      lspserver::TextEdit{.range = EditRange, .newText = std::move(NewText)});
  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  lspserver::WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};
  return lspserver::CodeAction{
      .title = Title,
      .kind = std::string(Kind),
      .edit = std::move(WE),
  };
}

bool isValidNixIdentifier(const std::string &S) {
  if (S.empty())
    return false;

  // Check first character: must be letter or underscore
  char First = S[0];
  if (!((First >= 'a' && First <= 'z') || (First >= 'A' && First <= 'Z') ||
        First == '_'))
    return false;

  // Check remaining characters
  for (size_t I = 1; I < S.size(); ++I) {
    char C = S[I];
    if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') ||
          (C >= '0' && C <= '9') || C == '_' || C == '-' || C == '\''))
      return false;
  }

  // Check against Nix keywords and reserved literals
  static const std::set<std::string> Keywords = {
      "if",  "then",    "else", "assert", "with",  "let", "in",
      "rec", "inherit", "or",   "true",   "false", "null"};
  return Keywords.find(S) == Keywords.end();
}

std::string escapeNixString(llvm::StringRef S) {
  std::string Result;
  Result.reserve(S.size() + S.size() / 4 + 2);
  for (size_t I = 0; I < S.size(); ++I) {
    char C = S[I];
    switch (C) {
    case '"':
      Result += "\\\"";
      break;
    case '\\':
      Result += "\\\\";
      break;
    case '\n':
      Result += "\\n";
      break;
    case '\r':
      Result += "\\r";
      break;
    case '\t':
      Result += "\\t";
      break;
    case '$':
      // Only escape ${ to prevent interpolation
      if (I + 1 < S.size() && S[I + 1] == '{') {
        Result += "\\${";
        ++I; // Skip the '{'
      } else {
        Result += C;
      }
      break;
    default:
      Result += C;
    }
  }
  return Result;
}

std::string quoteNixAttrKey(const std::string &Key) {
  if (isValidNixIdentifier(Key))
    return Key;

  return "\"" + escapeNixString(Key) + "\"";
}

} // namespace nixd
