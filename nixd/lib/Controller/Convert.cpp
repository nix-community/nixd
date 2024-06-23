#include "Convert.h"
#include "nixf/Basic/Diagnostic.h"

using namespace lspserver;

namespace nixd {

int getLSPSeverity(nixf::Diagnostic::DiagnosticKind Kind) {
  switch (nixf::Diagnostic::severity(Kind)) {
  case nixf::Diagnostic::DS_Fatal:
  case nixf::Diagnostic::DS_Error:
    return 1;
  case nixf::Diagnostic::DS_Warning:
    return 2;
  case nixf::Diagnostic::DS_Info:
    return 3;
  case nixf::Diagnostic::DS_Hint:
    return 4;
  }
  assert(false && "Invalid severity");
  __builtin_unreachable();
}

lspserver::Position toLSPPosition(const nixf::LexerCursor &P) {
  return lspserver::Position{static_cast<int>(P.line()),
                             static_cast<int>(P.column())};
}

nixf::Position toNixfPosition(const lspserver::Position &P) {
  return {P.line, P.character};
}

nixf::PositionRange toNixfRange(const lspserver::Range &P) {
  return {toNixfPosition(P.start), toNixfPosition(P.end)};
}

lspserver::Range toLSPRange(const nixf::LexerCursorRange &R) {
  return lspserver::Range{toLSPPosition(R.lCur()), toLSPPosition(R.rCur())};
}

llvm::SmallVector<lspserver::DiagnosticTag, 1>
toLSPTags(const std::vector<nixf::DiagnosticTag> &Tags) {
  llvm::SmallVector<lspserver::DiagnosticTag, 1> Result;
  Result.reserve(Tags.size());
  for (const nixf::DiagnosticTag &Tag : Tags) {
    switch (Tag) {
    case nixf::DiagnosticTag::Faded:
      Result.emplace_back(DiagnosticTag::Unnecessary);
      break;
    case nixf::DiagnosticTag::Striked:
      Result.emplace_back(DiagnosticTag::Deprecated);
      break;
    }
  }
  return Result;
}

} // namespace nixd
