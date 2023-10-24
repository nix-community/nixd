#pragma once

#include "nixf/Syntax/RawSyntax.h"

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

class TriviaPiece : public RawNode {
private:
  const TriviaKind Kind;
  const std::string Text;

public:
  TriviaPiece(TriviaKind Kind, std::string Text)
      : RawNode(SyntaxKind::SK_TriviaPiece), Kind(Kind), Text(std::move(Text)) {
    Length = Text.length();
  }
  void dump(std::ostream &OS, bool DiscardTrivia = true) const override;
};

class Trivia : public RawNode {
public:
  using TriviaPieces = std::vector<TriviaPiece>;

private:
  const TriviaPieces Pieces;

public:
  explicit Trivia(TriviaPieces Pieces);
  void dump(std::ostream &OS, bool DiscardTrivia = true) const override;
};

} // namespace nixf
