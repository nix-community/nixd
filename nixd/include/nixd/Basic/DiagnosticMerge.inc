// DiagnosticMerge.inc, merge all declarations

// provides: DIAG_ALL(SNAME, CNAME, SEVERITY, MESSAGE)

#ifdef DIAG_ALL

#define DIAG_SIMPLE(SNAME, CNAME, SEVERITY, MESSAGE)                           \
  DIAG_ALL(SNAME, CNAME, SEVERITY, MESSAGE)
#define DIAG_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)                       \
  DIAG_ALL(SNAME, CNAME, SEVERITY, MESSAGE)
#define DIAG_NOTE_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)                  \
  DIAG_ALL(SNAME, CNAME, SEVERITY, MESSAGE)
#define DIAG_NOTE(SNAME, CNAME, SEVERITY, MESSAGE)                             \
  DIAG_ALL(SNAME, CNAME, SEVERITY, MESSAGE)

#include "DiagnosticKinds.inc"

#undef DIAG_NOTE
#undef DIAG_NOTE_BODY
#undef DIAG_BODY
#undef DIAG_SIMPLE

#endif // DIAG_ALL
