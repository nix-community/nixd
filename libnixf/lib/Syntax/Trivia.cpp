#include "nixf/Syntax/Trivia.h"

#include <cassert>
#include <ostream>

namespace nixf {

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

void TriviaPiece::dump(std::ostream &OS) const { OS << Text; }

Trivia::Trivia(TriviaPieces P) : Pieces(std::move(P)) {
  Length = 0;
  for (const TriviaPiece &Piece : Pieces)
    Length += Piece.getLength();
}

void Trivia::dump(std::ostream &OS) const {
  for (const TriviaPiece &Piece : Pieces)
    Piece.dump(OS);
}

} // namespace nixf
