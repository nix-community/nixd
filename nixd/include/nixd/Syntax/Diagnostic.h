/// Nixd diagnostic types. Provide structured diagnostic with location, FixIt
/// hints, annotation, and notes
#pragma once

#include "nixd/Syntax/Range.h"

namespace nixd::syntax {

// The note.
struct Note {
  nixd::RangeIdx Range;

  std::string Msg;
};

struct Edit {
  nixd::RangeIdx Range;

  std::string NewText;
};

struct Diagnostic {
  nixd::RangeIdx Range;

  std::string Msg;

  enum DiagKind { Warning, Error } Kind;

  std::vector<Note> Notes;
  std::vector<Edit> Edits;
};

} // namespace nixd::syntax
