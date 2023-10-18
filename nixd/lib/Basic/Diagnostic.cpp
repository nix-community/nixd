#include "nixd/Basic/Diagnostic.h"
#include "nixd/Basic/Range.h"

namespace nixd {

nixd::Diagnostic::Severity nixd::Diagnostic::severity(DiagnosticKind Kind) {
  switch (Kind) {
#define DIAG(SNAME, CNAME, SEVERITY, MESSAGE)                                  \
  case DK_##CNAME:                                                             \
    return DS_##SEVERITY;
#include "nixd/Basic/DiagnosticKinds.inc"
#undef DIAG
  }
}
const char *nixd::Diagnostic::message(DiagnosticKind Kind) {
  switch (Kind) {
#define DIAG(SNAME, CNAME, SEVERITY, MESSAGE)                                  \
  case DK_##CNAME:                                                             \
    return MESSAGE;
#include "nixd/Basic/DiagnosticKinds.inc"
#undef DIAG
  }
}
const char *nixd::Diagnostic::sname(DiagnosticKind Kind) {
  switch (Kind) {
#define DIAG(SNAME, CNAME, SEVERITY, MESSAGE)                                  \
  case DK_##CNAME:                                                             \
    return SNAME;
#include "nixd/Basic/DiagnosticKinds.inc"
#undef DIAG
  }
}
const char *nixd::Note::message(NoteKind Kind) {
  switch (Kind) {
#define DIAG_NOTE(SNAME, CNAME, MESSAGE)                                       \
  case NK_##CNAME:                                                             \
    return MESSAGE;
#include "nixd/Basic/NoteKinds.inc"
#undef DIAG_NOTE
  }
}

} // namespace nixd
