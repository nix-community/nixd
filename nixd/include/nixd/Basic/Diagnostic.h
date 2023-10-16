/// Diagnostic.h, diagnostic types and definitions
///
/// Diagnostics are structures with a main message,
/// and optionally some additional information (body).
///
/// For diagnostics with a body,
/// they may need a special overrided function to format the message.
///
#pragma once

#include "Range.h"
#include <optional>
#include <string>

namespace nixd {

/// The super class for all diagnostics.
/// concret diagnostic types are defined in Diagnostic*.inc
struct Diagnostic {

  /// Location of this diagnostic
  RangeIdx Range;

  Diagnostic(RangeIdx Range) : Range(Range) {}

  /// Each diagnostic contains a severity field,
  /// should be "Fatal", "Error" or "Warning"
  /// this will affect the eval process.
  ///
  /// "Fatal"   -- shouldn't eval the code, e.g. parsing error.
  /// "Error"   -- trigger an error in nix, but we can eval the code.
  /// "Warning" -- just a warning.
  /// "Note"    -- some additional information about the error.
  enum Severity { DS_Fatal, DS_Error, DS_Warning, DS_Note };

  /// Internal diagnostic kind
  enum Kind {
#define DIAG_ALL(SNAME, CNAME, SEVERITY, MESSAGE) DK_##CNAME,
#include "DiagnosticMerge.inc"
#undef DIAG_ALL
  };

  [[nodiscard]] virtual Kind kind() const = 0;

  [[nodiscard]] virtual Severity severity() const = 0;

  /// Format a printable diagnostic
  [[nodiscard]] virtual std::string_view format() const = 0;

  /// Short name, switch name.
  /// There might be a human readable short name that controls the diagnostic
  /// For example, one may pass -Wno-dup-formal to suppress duplicated formals.
  /// A special case for parsing errors, generated from bison
  /// have the sname "bison"
  [[nodiscard]] virtual const char *sname() const = 0;

  virtual ~Diagnostic() = default;
};

struct NotableDiagnostic : Diagnostic {
  NotableDiagnostic(RangeIdx Range) : Diagnostic(Range) {}
  using NotesTy = std::vector<std::unique_ptr<Diagnostic>>;
  NotesTy Notes;
  NotesTy &notes() { return Notes; }
};

#define DIAG_SIMPLE(SNAME, CNAME, SEVERITY, MESSAGE)                           \
  struct Diag##CNAME : Diagnostic {                                            \
    Diag##CNAME(RangeIdx Range) : Diagnostic(Range) {}                         \
    std::string_view format() const override { return MESSAGE; }               \
    const char *sname() const override { return SNAME; }                       \
    Severity severity() const override { return DS_##SEVERITY; }               \
    Kind kind() const override { return DK_##CNAME; }                          \
  };
#include "DiagnosticKinds.inc"
#undef DIAG_SIMPLE

#define DIAG_NOTE(SNAME, CNAME, SEVERITY, MESSAGE)                             \
  struct Diag##CNAME : NotableDiagnostic {                                     \
    Diag##CNAME(RangeIdx Range) : NotableDiagnostic(Range) {}                  \
    std::string_view format() const override { return MESSAGE; }               \
    const char *sname() const override { return SNAME; }                       \
    Severity severity() const override { return DS_##SEVERITY; }               \
    Kind kind() const override { return DK_##CNAME; }                          \
  };
#include "DiagnosticKinds.inc"
#undef DIAG_NOTE

#define DIAG_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)                       \
  struct Diag##CNAME : Diagnostic BODY;
#include "DiagnosticKinds.inc"
#undef DIAG_BODY

#define DIAG_NOTE_BODY(SNAME, CNAME, SEVERITY, MESSAGE, BODY)                  \
  struct Diag##CNAME : NotableDiagnostic BODY;
#include "DiagnosticKinds.inc"
#undef DIAG_NOTE_BODY

} // namespace nixd
