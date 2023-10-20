#pragma once

#include <cassert>
#include <string>
#include <vector>

namespace nixf {

enum class TriviaKind {
  /// A space ' ' character.
  Space,

  /// A tab '\t' character.
  Tab,

  /// A vertical tab '\v' character.
  VerticalTab,

  /// A form-feed '\f' character.
  Formfeed,

  /// A newline '\n' character.
  Newline,

  /// A carriage return '\r' character.
  CarriageReturn,

  /// A line comment, starting with '#'
  LineComment,

  /// A block comment, starting with '/*' and ending with '*/'.
  BlockComment,
};

TriviaKind spaceTriviaKind(char Ch);

char spaceTriviaCh(TriviaKind Kind);

bool isSpaceTrivia(TriviaKind Kind);

struct TriviaPiece {
  TriviaKind Kind;
  unsigned Count;
  std::string Text;

  TriviaPiece(TriviaKind Kind, unsigned Count, std::string Text)
      : Kind(Kind), Count(Count), Text(std::move(Text)) {}

  static TriviaPiece spaces(unsigned Count) {
    return TriviaPiece{TriviaKind::Space, Count, std::string{}};
  }

  static TriviaPiece verticalTabs(unsigned Count) {
    return TriviaPiece{TriviaKind::VerticalTab, Count, std::string{}};
  }

  static TriviaPiece formFeeds(unsigned Count) {
    return TriviaPiece{TriviaKind::Formfeed, Count, std::string{}};
  }

  static TriviaPiece tabs(unsigned Count) {
    return TriviaPiece{TriviaKind::Tab, Count, std::string{}};
  }

  static TriviaPiece newlines(unsigned Count) {
    return TriviaPiece{TriviaKind::Newline, Count, std::string{}};
  }

  static TriviaPiece carriageReturns(unsigned Count) {
    return TriviaPiece{TriviaKind::CarriageReturn, Count, std::string{}};
  }

  static TriviaPiece lineComment(std::string Text) {
    return TriviaPiece{TriviaKind::LineComment, 1, std::move(Text)};
  }

  static TriviaPiece blockComment(std::string Text) {
    return TriviaPiece{TriviaKind::BlockComment, 1, std::move(Text)};
  }

  friend std::ostream &operator<<(std::ostream &, const TriviaPiece &Piece);
};

struct Trivia {
  using TriviaPieces = std::vector<TriviaPiece>;
  TriviaPieces Pieces;

  friend std::ostream &operator<<(std::ostream &, const Trivia &Trivia);
};

} // namespace nixf
