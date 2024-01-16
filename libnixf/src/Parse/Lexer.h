/// \file
/// \brief Lexer declaration. The lexer is a "stateful" lexer and highly tied to
/// parser.
///
/// This should be considered as implementation detail of the parser. So the
/// header is explicitly made private. Unit tests should be placed in the
/// lib/Parse/test directory.
#pragma once

#include "Token.h"

#include "nixf/Basic/Range.h"

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace nixf {

class DiagnosticEngine;

class Lexer {
  const std::string_view Src;
  DiagnosticEngine &Diags;

  Point Cur;

  void consume(std::size_t N = 1) {
    assert(Cur.Offset + N <= Src.length());
    // Update Line & Column & Offset
    for (std::size_t I = 0; I < N; ++I) {
      if (Src[Cur.Offset + N] == '\n') {
        ++Cur.Line;
        Cur.Column = 0;
      } else {
        ++Cur.Column;
      }
    }
    Cur.Offset += N;
  }

  // token recorder
  Point TokStartPtr;
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
  std::optional<RangeTy> consumePrefix(std::string_view Prefix);

  bool consumeOne(char C);

  std::optional<char> consumeOneOf(std::string_view Chars);

  std::optional<RangeTy> consumeManyOf(std::string_view Chars);

  std::optional<RangeTy> consumeManyDigits() {
    return consumeManyOf("0123456789");
  }

  std::optional<RangeTy> consumeManyPathChar();

  /// Look ahead and check if we has \p Prefix
  bool peekPrefix(std::string_view Prefix);

  bool consumePathStart();

  bool consumeURI();

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

  [[nodiscard]] RangeTy curRange() const { return {Cur, Cur}; }

  [[nodiscard]] char peekUnwrap() const { return Src[Cur.Offset]; }

  [[nodiscard]] std::optional<char> peek() const {
    if (eof())
      return std::nullopt;
    return peekUnwrap();
  }

public:
  Lexer(std::string_view Src, DiagnosticEngine &Diags)
      : Src(Src), Diags(Diags), Cur() {}

  /// Reset the cursor at source \p offset (zero-based indexing)
  void setCur(const Point &NewCur) {
    assert(Src.begin() + NewCur.Offset <= Src.end());
    Cur = NewCur;
  }

  [[nodiscard]] const Point &cur() const { return Cur; }

  Token lex();
  Token lexString();
  Token lexIndString();
  Token lexPath();
};

} // namespace nixf
