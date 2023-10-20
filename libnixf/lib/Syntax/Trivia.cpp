#include "nixf/Syntax/Trivia.h"

#include <ostream>

namespace nixf {

static std::ostream &printChCount(std::ostream &OS, char Ch, unsigned Count) {
  for (unsigned I = 0; I < Count; I++) {
    OS << Ch;
  }
  return OS;
}

TriviaKind spaceTriviaKind(char Ch) {
  assert(std::isspace(Ch));
  switch (Ch) {
  case ' ':
    return TriviaKind::Space;
  case '\n':
    return TriviaKind::Newline;
  case '\r':
    return TriviaKind::CarriageReturn;
  case '\f':
    return TriviaKind::Formfeed;
  case '\t':
    return TriviaKind::Tab;
  case '\v':
    return TriviaKind::VerticalTab;
  default:
    __builtin_unreachable();
  }
}

bool isSpaceTrivia(TriviaKind Kind) {
  return TriviaKind::Space <= Kind && Kind < TriviaKind::LineComment;
}

char spaceTriviaCh(TriviaKind Kind) {
  switch (Kind) {
  case TriviaKind::Space:
    return ' ';
  case TriviaKind::Tab:
    return '\t';
  case TriviaKind::VerticalTab:
    return '\v';
  case TriviaKind::Formfeed:
    return '\f';
  case TriviaKind::Newline:
    return '\n';
  case TriviaKind::CarriageReturn:
    return '\r';
  default:
    __builtin_unreachable();
  }
}

std::ostream &operator<<(std::ostream &OS, const TriviaPiece &Piece) {
  if (isSpaceTrivia(Piece.Kind))
    return printChCount(OS, spaceTriviaCh(Piece.Kind), Piece.Count);
  OS << Piece.Text;
  return OS;
}

std::ostream &operator<<(std::ostream &OS, const Trivia &Trivia) {
  for (const TriviaPiece &Piece : Trivia.Pieces)
    OS << Piece;

  return OS;
}

} // namespace nixf
