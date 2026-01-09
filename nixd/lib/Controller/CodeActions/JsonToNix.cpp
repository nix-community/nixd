/// \file
/// \brief Implementation of JSON to Nix conversion code action.

#include "JsonToNix.h"
#include "Utils.h"

#include <llvm/Support/JSON.h>
#include <lspserver/SourceCode.h>

#include <iomanip>
#include <sstream>

namespace nixd {

namespace {

/// \brief Convert a JSON value to Nix expression syntax.
/// \param V The JSON value to convert
/// \param Indent Current indentation level (for pretty-printing)
/// \param Depth Current recursion depth (safety limit)
/// \return Nix expression string, or empty string on error
std::string jsonToNix(const llvm::json::Value &V, size_t Indent = 0,
                      size_t Depth = 0) {
  if (Depth > MaxJsonDepth)
    return ""; // Safety limit exceeded

  std::string IndentStr(Indent * 2, ' ');
  std::string NextIndent((Indent + 1) * 2, ' ');
  std::string Out;

  if (V.kind() == llvm::json::Value::Null) {
    Out = "null";
  } else if (auto B = V.getAsBoolean()) {
    Out = *B ? "true" : "false";
  } else if (auto I = V.getAsInteger()) {
    Out = std::to_string(*I);
  } else if (auto D = V.getAsNumber()) {
    // Format floating point with enough precision
    std::ostringstream SS;
    SS << std::setprecision(17) << *D;
    Out = SS.str();
  } else if (auto S = V.getAsString()) {
    Out = "\"" + escapeNixString(*S) + "\"";
  } else if (const auto *A = V.getAsArray()) {
    if (A->size() > MaxJsonWidth)
      return ""; // Width limit exceeded
    if (A->empty()) {
      Out = "[ ]";
    } else {
      // Pre-allocate memory to reduce reallocations
      // Estimate: opening + closing + elements * (indent + value_estimate +
      // newline)
      size_t EstimatedSize = 4 + A->size() * ((Indent + 1) * 2 + 20);
      Out.reserve(EstimatedSize);
      Out = "[\n";
      for (size_t I = 0; I < A->size(); ++I) {
        std::string Elem = jsonToNix((*A)[I], Indent + 1, Depth + 1);
        if (Elem.empty())
          return ""; // Propagate error
        Out += NextIndent + Elem;
        if (I + 1 < A->size())
          Out += "\n";
      }
      Out += "\n" + IndentStr + "]";
    }
  } else if (const auto *O = V.getAsObject()) {
    if (O->size() > MaxJsonWidth)
      return ""; // Width limit exceeded
    if (O->empty()) {
      Out = "{ }";
    } else {
      // Pre-allocate memory to reduce reallocations
      // Estimate: braces + elements * (indent + key + " = " + value_estimate +
      // ";\n")
      size_t EstimatedSize = 4 + O->size() * ((Indent + 1) * 2 + 30);
      Out.reserve(EstimatedSize);
      Out = "{\n";
      size_t I = 0;
      for (const auto &KV : *O) {
        std::string Key = quoteNixAttrKey(KV.first.str());
        std::string Val = jsonToNix(KV.second, Indent + 1, Depth + 1);
        if (Val.empty())
          return ""; // Propagate error
        Out += NextIndent + Key + " = " + Val + ";";
        if (I + 1 < O->size())
          Out += "\n";
        ++I;
      }
      Out += "\n" + IndentStr + "}";
    }
  }
  return Out;
}

} // namespace

void addJsonToNixAction(llvm::StringRef Src, const lspserver::Range &Range,
                        const std::string &FileURI,
                        std::vector<lspserver::CodeAction> &Actions) {
  // Convert LSP positions to byte offsets
  llvm::Expected<size_t> StartOffset =
      lspserver::positionToOffset(Src, Range.start);
  llvm::Expected<size_t> EndOffset =
      lspserver::positionToOffset(Src, Range.end);

  if (!StartOffset || !EndOffset) {
    if (!StartOffset)
      llvm::consumeError(StartOffset.takeError());
    if (!EndOffset)
      llvm::consumeError(EndOffset.takeError());
    return;
  }

  // Validate range
  if (*StartOffset >= *EndOffset || *EndOffset > Src.size())
    return;

  // Extract selected text
  llvm::StringRef SelectedText =
      Src.substr(*StartOffset, *EndOffset - *StartOffset);

  // Skip if selection is too short (minimum valid JSON is "{}" or "[]")
  if (SelectedText.size() < 2)
    return;

  // Skip if first character is not { or [ (quick rejection)
  char First = SelectedText.front();
  if (First != '{' && First != '[')
    return;

  // Try to parse as JSON
  llvm::Expected<llvm::json::Value> JsonVal = llvm::json::parse(SelectedText);
  if (!JsonVal) {
    llvm::consumeError(JsonVal.takeError());
    return;
  }

  // Skip empty JSON structures - already valid Nix
  if (const auto *A = JsonVal->getAsArray()) {
    if (A->empty())
      return;
  } else if (const auto *O = JsonVal->getAsObject()) {
    if (O->empty())
      return;
  }

  // Convert JSON to Nix
  std::string NixText = jsonToNix(*JsonVal);
  if (NixText.empty())
    return;

  Actions.emplace_back(createSingleEditAction(
      "Convert JSON to Nix", lspserver::CodeAction::REFACTOR_REWRITE_KIND,
      FileURI, Range, std::move(NixText)));
}

} // namespace nixd
