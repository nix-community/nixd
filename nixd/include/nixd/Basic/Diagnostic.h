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

#include <fmt/args.h>

namespace nixd {

class PartialDiagnostic {
public:
  virtual const char *format() const {
    if (Result.empty())
      Result = fmt::vformat(message(), Args);
    return Result.c_str();
  };

  [[nodiscard]] virtual const char *message() const = 0;

  virtual ~PartialDiagnostic() = default;

  template <class T> PartialDiagnostic &operator<<(const T &Var) {
    Args.push_back(Var);
    return *this;
  }

protected:
  mutable std::string Result;
  fmt::dynamic_format_arg_store<fmt::format_context> Args;
  /// Location of this diagnostic
  RangeIdx Range;
};

class Note : public PartialDiagnostic {
public:
  /// Internal kind
  enum NoteKind {
#define DIAG_NOTE(SNAME, CNAME, MESSAGE) NK_##CNAME,
#include "NoteKinds.inc"
#undef DIAG_NOTE
  };

  Note(RangeIdx Range, NoteKind Kind) : Range(Range), Kind(Kind) {}

  template <class T> PartialDiagnostic &operator<<(const T &Var) {
    Args.push_back(Var);
    return *this;
  }

  NoteKind kind() const { return Kind; }

  [[nodiscard]] static const char *message(NoteKind Kind);

  [[nodiscard]] const char *message() const override { return message(kind()); }

  RangeIdx range() const { return Range; }

private:
  RangeIdx Range;
  NoteKind Kind;
};

/// The super class for all diagnostics.
/// concret diagnostic types are defined in Diagnostic*.inc
class Diagnostic : public PartialDiagnostic {
public:
  /// Each diagnostic contains a severity field,
  /// should be "Fatal", "Error" or "Warning"
  /// this will affect the eval process.
  ///
  /// "Fatal"   -- shouldn't eval the code, e.g. parsing error.
  /// "Error"   -- trigger an error in nix, but we can eval the code.
  /// "Warning" -- just a warning.
  /// "Note"    -- some additional information about the error.
  enum Severity { DS_Fatal, DS_Error, DS_Warning };

  /// Internal kind
  enum DiagnosticKind {
#define DIAG(SNAME, CNAME, SEVERITY, MESSAGE) DK_##CNAME,
#include "DiagnosticKinds.inc"
#undef DIAG
  };

  Diagnostic(RangeIdx Range, DiagnosticKind Kind) : Range(Range), Kind(Kind) {}

  [[nodiscard]] DiagnosticKind kind() const { return Kind; };

  [[nodiscard]] static Severity severity(DiagnosticKind Kind);

  [[nodiscard]] static const char *message(DiagnosticKind Kind);

  [[nodiscard]] const char *message() const override { return message(kind()); }

  /// Short name, switch name.
  /// There might be a human readable short name that controls the diagnostic
  /// For example, one may pass -Wno-dup-formal to suppress duplicated formals.
  /// A special case for parsing errors, generated from bison
  /// have the sname "bison"
  [[nodiscard]] static const char *sname(DiagnosticKind Kind);

  [[nodiscard]] virtual const char *sname() const { return sname(kind()); }

  Note &note(RangeIdx Range, Note::NoteKind Kind) {
    return *Notes.emplace_back(std::make_unique<Note>(Range, Kind));
  }

  std::vector<std::unique_ptr<Note>> &notes() { return Notes; }

  RangeIdx range() const { return Range; }

private:
  /// Location of this diagnostic
  RangeIdx Range;

  std::vector<std::unique_ptr<Note>> Notes;

  DiagnosticKind Kind;
};

class DiagnosticEngine {
public:
  Diagnostic &diag(RangeIdx Range, Diagnostic::DiagnosticKind Kind) {
    auto Diag = std::make_unique<Diagnostic>(Range, Kind);
    switch (getServerity(Diag->kind())) {
    case Diagnostic::DS_Fatal:
      return *Fatals.emplace_back(std::move(Diag));
    case Diagnostic::DS_Error:
      return *Errors.emplace_back(std::move(Diag));
    case Diagnostic::DS_Warning:
      return *Warnings.emplace_back(std::move(Diag));
    }
  }

  std::vector<std::unique_ptr<Diagnostic>> &warnings() { return Warnings; }
  std::vector<std::unique_ptr<Diagnostic>> &errors() { return Errors; }
  std::vector<std::unique_ptr<Diagnostic>> &fatals() { return Fatals; }

private:
  Diagnostic::Severity getServerity(Diagnostic::DiagnosticKind Kind) {
    return Diagnostic::severity(Kind);
  }
  std::vector<std::unique_ptr<Diagnostic>> Warnings;
  std::vector<std::unique_ptr<Diagnostic>> Errors;
  std::vector<std::unique_ptr<Diagnostic>> Fatals;
};

} // namespace nixd
