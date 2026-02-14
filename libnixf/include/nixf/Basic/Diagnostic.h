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
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nixf {

/// Remove the text at `OldRange`, and replace it as `NewText`
/// Special cases:
/// 1. Insertions: special `OldRange` that `Begin` == `End`.
/// 2. Removals:   empty `NewText`.
class TextEdit {
  LexerCursorRange OldRange;
  std::string NewText;

public:
  TextEdit(LexerCursorRange OldRange, std::string NewText)
      : OldRange(OldRange), NewText(std::move(NewText)) {
    assert(OldRange.lCur() != OldRange.rCur() || !this->NewText.empty());
  }

  static TextEdit mkInsertion(LexerCursor P, std::string NewText) {
    return {{P, P}, std::move(NewText)};
  }

  static TextEdit mkRemoval(LexerCursorRange RemovingRange) {
    return {RemovingRange, ""};
  }

  [[nodiscard]] bool isReplace() const {
    return !isRemoval() && !isInsertion();
  }

  [[nodiscard]] bool isRemoval() const { return NewText.empty(); }

  [[nodiscard]] bool isInsertion() const {
    return OldRange.lCur() == OldRange.rCur();
  }

  [[nodiscard]] LexerCursorRange oldRange() const { return OldRange; }
  [[nodiscard]] std::string_view newText() const { return NewText; }
};

class Fix {
  std::vector<TextEdit> Edits;
  std::string Message;
  bool Preferred = false;

public:
  Fix(std::vector<TextEdit> Edits, std::string Message)
      : Edits(std::move(Edits)), Message(std::move(Message)) {}

  Fix &edit(TextEdit Edit) {
    Edits.emplace_back(std::move(Edit));
    return *this;
  }

  [[nodiscard]] const std::vector<TextEdit> &edits() const { return Edits; }
  [[nodiscard]] const std::string &message() const { return Message; }
  Fix &setPreferred(bool P = true) {
    Preferred = P;
    return *this;
  }
  [[nodiscard]] bool isPreferred() const { return Preferred; }
};

enum class DiagnosticTag {
  Faded,
  Striked,
};

class PartialDiagnostic {
public:
  [[nodiscard]] virtual const char *message() const = 0;

  virtual ~PartialDiagnostic() = default;

  PartialDiagnostic &operator<<(std::string Var) {
    Args.emplace_back(std::move(Var));
    return *this;
  }

  [[nodiscard]] std::string format() const;

  [[nodiscard]] const std::vector<std::string> &args() const { return Args; }

  std::vector<std::string> &args() { return Args; }

  void tag(DiagnosticTag Tag) { Tags.push_back(Tag); }

  [[nodiscard]] const std::vector<DiagnosticTag> &tags() const { return Tags; }

  [[nodiscard]] LexerCursorRange range() const { return Range; }

protected:
  PartialDiagnostic() = default;

  PartialDiagnostic(LexerCursorRange Range) : Range(Range) {}

private:
  std::vector<DiagnosticTag> Tags;
  std::vector<std::string> Args;
  /// Location of this diagnostic
  LexerCursorRange Range;
};

class Note : public PartialDiagnostic {
public:
  /// Internal kind
  enum NoteKind {
#define DIAG_NOTE(SNAME, CNAME, MESSAGE) NK_##CNAME,
#include "NoteKinds.inc"
#undef DIAG_NOTE
  };

  Note(NoteKind Kind, LexerCursorRange Range)
      : PartialDiagnostic(Range), Kind(Kind) {}

  template <class T> PartialDiagnostic &operator<<(const T &Var) {
    args().push_back(Var);
    return *this;
  }

  [[nodiscard]] static const char *sname(NoteKind Kind);

  [[nodiscard]] virtual const char *sname() const { return sname(kind()); }

  [[nodiscard]] NoteKind kind() const { return Kind; }

  [[nodiscard]] static const char *message(NoteKind Kind);

  [[nodiscard]] const char *message() const override { return message(kind()); }

private:
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
  /// "Warning" -- just a warning.
  enum Severity {
    /// shouldn't eval the code, e.g. parsing error.
    DS_Fatal,
    /// trigger an error in nix, but we can recover & eval the code.
    DS_Error,
    /// A warning.
    DS_Warning,
    /// An information.
    DS_Info,

    /// A hint. Hints are usually not rendered directly in some editor GUI
    /// So this is suitable for liveness analysis results.
    /// For example, "unused xxx"
    DS_Hint,
  };

  /// Internal kind
#include "DiagnosticEnum.h"

  Diagnostic(DiagnosticKind Kind, LexerCursorRange Range)
      : PartialDiagnostic(Range), Kind(Kind) {}

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

  /// \brief Parse diagnostic "cname" to diagnostic id
  [[nodiscard]] static std::optional<Diagnostic::DiagnosticKind>
  parseKind(std::string_view SName);

  [[nodiscard]] virtual const char *sname() const { return sname(kind()); }

  Note &note(Note::NoteKind Kind, LexerCursorRange Range) {
    return Notes.emplace_back(Kind, Range);
  }

  [[nodiscard]] const std::vector<Note> &notes() const { return Notes; }

  Fix &fix(std::string Message) {
    return Fixes.emplace_back(Fix{{}, std::move(Message)});
  }

  [[nodiscard]] const std::vector<Fix> &fixes() const { return Fixes; }

private:
  DiagnosticKind Kind;

  std::vector<Note> Notes;
  std::vector<Fix> Fixes;
};

} // namespace nixf
