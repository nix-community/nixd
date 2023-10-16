#include "nixd/Basic/Diagnostic.h"
#include "nixd/Basic/Range.h"

#include "llvm/Support/FormatVariadic.h"

namespace nixd {

#define DIAG_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)                       \
  const char *Diag##CNAME::sname() const { return SNAME; }                     \
  Diagnostic::Severity Diag##CNAME::severity() const { return DS_##SEVERITY; } \
  Diagnostic::Kind Diag##CNAME::kind() const { return DK_##CNAME; }            \
  constexpr const char *Diag##CNAME::message() { return MESSAGE; }             \
  std::string_view Diag##CNAME::format() const { return Str; }
#define DIAG_NOTE_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)                  \
  DIAG_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)
#include "nixd/Basic/DiagnosticKinds.inc"
#undef DIAG_BODY
#undef DIAG_NOTE_BODY

DiagBisonParse::DiagBisonParse(RangeIdx Range, std::string Str)
    : Diagnostic(Range), Str(std::move(Str)) {}

DiagDuplicatedAttr::DiagDuplicatedAttr(RangeIdx Range, std::string AttrName)
    : NotableDiagnostic(Range) {
  Str = llvm::formatv(message(), AttrName);
}

static const char *getPrefix(bool P) { return P ? "" : "non-"; }

DiagThisRecursive::DiagThisRecursive(RangeIdx Range, bool Recursive)
    : Diagnostic(Range) {
  Str = llvm::formatv(message(), getPrefix(Recursive));
}

DiagRecConsider::DiagRecConsider(RangeIdx Range, bool MarkedRecursive)
    : Diagnostic(Range) {
  Str = llvm::formatv(message(), getPrefix(MarkedRecursive),
                      getPrefix(!MarkedRecursive));
}

DiagInvalidInteger::DiagInvalidInteger(RangeIdx Range, std::string_view Content)
    : Diagnostic(Range) {
  Str = llvm::formatv(message(), Content);
}

DiagInvalidFloat::DiagInvalidFloat(RangeIdx Range,
                                   const std::string_view Content)
    : Diagnostic(Range) {
  Str = llvm::formatv(message(), Content);
}

} // namespace nixd
