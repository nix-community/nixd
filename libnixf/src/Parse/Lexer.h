/// \file
/// \brief Lexer declaration. The lexer is a "stateful" lexer and highly tied to
/// parser.
///
/// This should be considered as implementation detail of the parser. So the
/// header is explicitly made private. Unit tests should be placed in the
/// lib/Parse/test directory.
#pragma once

#include "Token.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Range.h"

#include <cassert>
#include <optional>
#include <string_view>

namespace nixf {

class Lexer {
  const std::string_view Src;
  std::vector<Diagnostic> &Diags;

  LexerCursor Cur;

  void consume(std::size_t N = 1) {
    assert(Cur.Offset + N <= Src.length());
    // Update Line & Column & Offset
    for (std::size_t I = 0; I < N; ++I) {
      if (Src[Cur.Offset + I] == '\n') {
        ++Cur.Line;
        Cur.Column = 0;
      } else {
        ++Cur.Column;
      }
    }
    Cur.Offset += N;
  }

  // token recorder
  LexerCursor TokStartPtr;
  tok::TokenKind Tok;
  void startToken() {
    Tok = tok::tok_unknown;
    TokStartPtr = Cur;
  }
  Token finishToken() {
    return {
        Tok,
        {TokStartPtr, Cur},
        Src.substr(TokStartPtr.Offset, Cur.Offset - TokStartPtr.Offset),
    };
  }

  void consumeTrivia();

  bool consumeWhitespaces();
  bool consumeComments();

  [[nodiscard]] bool eof(std::size_t Offset) const {
    return Offset >= Src.length();
  }

  [[nodiscard]] bool eof() const { return eof(Cur.Offset); }

  bool consumeEOL() { return consumePrefix("\r\n") || consumePrefix("\n"); }

  bool lexFloatExp();

  // Advance cursor if it starts with prefix, otherwise do nothing
  std::optional<LexerCursorRange> consumePrefix(std::string_view Prefix);

  bool consumeOne(char C);

  std::optional<char> consumeOneOf(std::string_view Chars);

  std::optional<LexerCursorRange> consumeManyOf(std::string_view Chars);

  std::optional<LexerCursorRange> consumeManyDigits() {
    return consumeManyOf("0123456789");
  }

  std::optional<LexerCursorRange> consumeManyPathChar();

  /// Look ahead and check if we has \p Prefix
  bool peekPrefix(std::string_view Prefix);

  bool consumePathStart();

  bool consumeURI();

  bool consumeSPath();

  /// Should be called after lexing a "raw" identifier, we check if it is a
  /// keyword and make assignment: `Tok <- tok_kw_*`
  void maybeKW();

  void lexIdentifier();

  void lexNumbers();

  [[nodiscard]] std::string_view tokStr() const {
    return Src.substr(TokStartPtr.Offset, Cur.Offset - TokStartPtr.Offset);
  }

  [[nodiscard]] std::string_view remain() const {
    return Src.substr(Cur.Offset);
  }

  [[nodiscard]] LexerCursorRange curRange() const { return {Cur, Cur}; }

  [[nodiscard]] char peekUnwrap() const { return Src[Cur.Offset]; }

  [[nodiscard]] std::optional<char> peek() const {
    if (eof())
      return std::nullopt;
    return peekUnwrap();
  }

public:
  Lexer(std::string_view Src, std::vector<Diagnostic> &Diags)
      : Src(Src), Diags(Diags), Cur() {}

  /// Reset the cursor at source \p offset (zero-based indexing)
  void setCur(const LexerCursor &NewCur) {
    assert(Src.begin() + NewCur.Offset <= Src.end());
    Cur = NewCur;
  }

  [[nodiscard]] const LexerCursor &cur() const { return Cur; }

  Token lex();
  Token lexString();
  Token lexIndString();
  Token lexPath();
};

} // namespace nixf
