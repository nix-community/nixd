#pragma once

#include "Token.h"

#include "nixf/Basic/Range.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string_view>

namespace nixf {

class DiagnosticEngine;

class Lexer {
  const std::string_view Src;
  DiagnosticEngine &Diags;
  const char *Cur;

  // token recorder
  const char *TokStartPtr;
  tok::TokenKind Tok;
  void startToken() {
    Tok = tok::tok_unknown;
    TokStartPtr = Cur;
  }
  Token finishToken() { return {Tok, {TokStartPtr, Cur}}; }

  void consumeTrivia();

  bool consumeWhitespaces();
  bool consumeComments();

  bool eof(const char *Ptr) { return Ptr >= Src.end(); }

  bool eof() { return eof(Cur); }

  bool consumeEOL() { return consumePrefix("\r\n") || consumePrefix("\n"); }

  bool lexFloatExp();

  // Advance cursor if it starts with prefix, otherwise do nothing
  bool consumePrefix(std::string_view Prefix);

  /// Look ahead and check if we has \p Prefix
  bool prefix(std::string_view Prefix);

  /// Look ahead to see it is a path, paths has higher priority than identifiers
  /// If it is a valid path, \returns ending cursor
  /// Otherwise \returns nullptr
  const char *checkPathStart();

  /// Look ahead to see it is a URI.
  /// If it is a valid URI, \returns ending cursor
  /// Otherwise \returns nullptr
  const char *checkUriStart();

  /// Should be called after lexing a "raw" identifier, we check if it is a
  /// keyword and make assignment: `Tok <- tok_kw_*`
  void maybeKW();

  void lexIdentifier();

  void lexNumbers();

  [[nodiscard]] std::string_view tokStr() const { return {TokStartPtr, Cur}; }

  [[nodiscard]] std::string_view remain() const { return {Cur, Src.end()}; }

public:
  Lexer(std::string_view Src, DiagnosticEngine &Diags)
      : Src(Src), Diags(Diags) {
    Cur = Src.begin();
  }

  const char *cur() { return Cur; }

  /// Reset the cursor at source \p offset (zero-based indexing)
  void setCur(const char *NewCur) {
    assert(NewCur < Src.end());
    Cur = NewCur;
  }

  Token lex();
  Token lexString();
  Token lexIndString();
  Token lexPath();
};

} // namespace nixf
