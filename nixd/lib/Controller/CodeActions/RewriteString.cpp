/// \file
/// \brief Implementation of string literal syntax conversion code action.

#include "RewriteString.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Simple.h>

namespace nixd {

namespace {

/// \brief Check if the string at the given source position is indented style.
///
/// Indented strings start with '' while double-quoted strings start with ".
bool isIndentedString(llvm::StringRef Src, const nixf::LexerCursorRange &Range) {
  size_t Offset = Range.lCur().offset();
  if (Offset + 1 >= Src.size())
    return false;
  return Src[Offset] == '\'' && Src[Offset + 1] == '\'';
}

/// \brief Escape a string for indented string literal syntax.
///
/// Escapes:
/// - '' -> ''' (two single quotes become three)
/// - ${ -> ''${ (prevent interpolation)
std::string escapeForIndentedString(llvm::StringRef S) {
  std::string Result;
  Result.reserve(S.size() + S.size() / 8);

  for (size_t I = 0; I < S.size(); ++I) {
    char C = S[I];

    // Check for '' sequence that needs escaping
    if (C == '\'' && I + 1 < S.size() && S[I + 1] == '\'') {
      Result += "'''";
      ++I; // Skip the second quote
      continue;
    }

    // Check for ${ that needs escaping
    if (C == '$' && I + 1 < S.size() && S[I + 1] == '{') {
      Result += "''${";
      ++I; // Skip the '{'
      continue;
    }

    Result += C;
  }

  return Result;
}

/// \brief Build a double-quoted string from InterpolatedParts.
///
/// Iterates through fragments, escaping text parts and preserving
/// interpolations from source.
std::string buildDoubleQuotedString(const nixf::InterpolatedParts &Parts,
                                    llvm::StringRef Src) {
  std::string Result = "\"";

  for (const auto &Frag : Parts.fragments()) {
    if (Frag.kind() == nixf::InterpolablePart::SPK_Escaped) {
      // Escape the unescaped content for double-quoted string
      Result += escapeNixString(Frag.escaped());
    } else {
      // SPK_Interpolation: preserve source text exactly
      const nixf::Interpolation &Interp = Frag.interpolation();
      Result += Interp.src(Src);
    }
  }

  Result += "\"";
  return Result;
}

/// \brief Build an indented string from InterpolatedParts.
///
/// Iterates through fragments, escaping text parts and preserving
/// interpolations from source.
std::string buildIndentedString(const nixf::InterpolatedParts &Parts,
                                llvm::StringRef Src) {
  std::string Result = "''";

  for (const auto &Frag : Parts.fragments()) {
    if (Frag.kind() == nixf::InterpolablePart::SPK_Escaped) {
      // Escape the unescaped content for indented string
      Result += escapeForIndentedString(Frag.escaped());
    } else {
      // SPK_Interpolation: preserve source text exactly
      const nixf::Interpolation &Interp = Frag.interpolation();
      Result += Interp.src(Src);
    }
  }

  Result += "''";
  return Result;
}

} // namespace

void addRewriteStringAction(const nixf::Node &N,
                            const nixf::ParentMapAnalysis &PM,
                            const std::string &FileURI, llvm::StringRef Src,
                            std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside an ExprString
  const nixf::Node *StringNode = PM.upTo(N, nixf::Node::NK_ExprString);
  if (!StringNode)
    return;

  const auto &Str = static_cast<const nixf::ExprString &>(*StringNode);

  // Determine current string type
  bool IsIndented = isIndentedString(Src, Str.range());

  // Build the converted string
  std::string NewText;
  std::string Title;

  if (IsIndented) {
    // Convert indented -> double-quoted
    NewText = buildDoubleQuotedString(Str.parts(), Src);
    Title = "Convert to double-quoted string";
  } else {
    // Convert double-quoted -> indented
    NewText = buildIndentedString(Str.parts(), Src);
    Title = "Convert to indented string";
  }

  Actions.emplace_back(createSingleEditAction(
      Title, lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
      toLSPRange(Src, Str.range()), std::move(NewText)));
}

} // namespace nixd
