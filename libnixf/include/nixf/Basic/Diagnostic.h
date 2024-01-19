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

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nixf {

/// Fix-it hints. Remove the text at `OldRange`, and replace it as `NewText`
/// Special cases:
/// 1. Insertions: special `OldRange` that `Begin` == `End`.
/// 2. Removals:   empty `NewText`.
class Fix {
  RangeTy OldRange;
  std::string NewText;

public:
  Fix(RangeTy OldRange, std::string NewText)
      : OldRange(OldRange), NewText(std::move(NewText)) {
    assert(OldRange.begin() != OldRange.end() || !this->NewText.empty());
  }

  static Fix mkInsertion(Point P, std::string NewText) {
    return {{P, P}, std::move(NewText)};
  }

  static Fix mkRemoval(RangeTy RemovingRange) { return {RemovingRange, ""}; }

  [[nodiscard]] bool isReplace() const {
    return !isRemoval() && !isInsertion();
  }

  [[nodiscard]] bool isRemoval() const { return NewText.empty(); }

  [[nodiscard]] bool isInsertion() const {
    return OldRange.begin() == OldRange.end();
  }

  [[nodiscard]] RangeTy oldRange() const { return OldRange; }
  [[nodiscard]] std::string_view newText() const { return NewText; }
};

class PartialDiagnostic {
public:
  [[nodiscard]] virtual const char *message() const = 0;

  virtual ~PartialDiagnostic() = default;

  PartialDiagnostic &operator<<(std::string Var) {
    Args.emplace_back(std::move(Var));
    return *this;
  }

  [[nodiscard]] const std::vector<std::string> &args() const { return Args; }

protected:
  std::vector<std::string> Args;
  /// Location of this diagnostic
  RangeTy Range;
};

class Note : public PartialDiagnostic {
public:
  /// Internal kind
  enum NoteKind {
#define DIAG_NOTE(SNAME, CNAME, MESSAGE) NK_##CNAME,
#include "NoteKinds.inc"
#undef DIAG_NOTE
  };

  Note(NoteKind Kind, RangeTy Range) : Kind(Kind), Range(Range) {}

  template <class T> PartialDiagnostic &operator<<(const T &Var) {
    Args.push_back(Var);
    return *this;
  }

  NoteKind kind() const { return Kind; }

  [[nodiscard]] static const char *message(NoteKind Kind);

  [[nodiscard]] const char *message() const override { return message(kind()); }

  RangeTy range() const { return Range; }

private:
  NoteKind Kind;
  RangeTy Range;
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
  /// "Error"   -- trigger an error in nix, but we can recover & eval the code.
  /// "Warning" -- just a warning.
  /// "Note"    -- some additional information about the error.
  enum Severity { DS_Fatal, DS_Error, DS_Warning };

  /// Internal kind
  enum DiagnosticKind {
#define DIAG(SNAME, CNAME, SEVERITY, MESSAGE) DK_##CNAME,
#include "DiagnosticKinds.inc"
#undef DIAG
  };

  Diagnostic(DiagnosticKind Kind, RangeTy Range) : Kind(Kind), Range(Range) {}

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

  Note &note(Note::NoteKind Kind, RangeTy Range) {
    return Notes.emplace_back(Kind, Range);
  }

  std::vector<Note> &notes() { return Notes; }

  Diagnostic &fix(Fix F) {
    Fixes.emplace_back(std::move(F));
    return *this;
  }

  const std::vector<Fix> &fixes() { return Fixes; }

  [[nodiscard]] RangeTy range() const { return Range; }

private:
  DiagnosticKind Kind;
  /// Location of this diagnostic
  RangeTy Range;

  std::vector<Note> Notes;
  std::vector<Fix> Fixes;
};

} // namespace nixf
