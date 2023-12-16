#pragma once

#include "nixf/Basic/Range.h"
#include "nixf/Syntax/Token.h"
#include "nixf/Syntax/Trivia.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string_view>

namespace nixf {

class DiagnosticEngine;

class TokenView {
  const Token *Tok;
  const char *TokBegin;
  const char *TokEnd;

public:
  TokenView(const Token *Tok, const char *TB, const char *TE)
      : Tok(Tok), TokBegin(TB), TokEnd(TE) {}

  const Token *operator->() const { return Tok; }
  operator const Token *() const { return Tok; }

  [[nodiscard]] const char *getTokBegin() const { return TokBegin; }
  [[nodiscard]] const char *getTokEnd() const { return TokEnd; }

  [[nodiscard]] OffsetRange getTokRange() const { return {TokBegin, TokEnd}; }
};

class TokenAbs {
  std::unique_ptr<Token> Tok;

  const char *TokBegin;
  const char *TokEnd;

public:
  TokenAbs(std::unique_ptr<Token> Tok, const char *TB, const char *TE)
      : Tok(std::move(Tok)), TokBegin(TB), TokEnd(TE) {}

  [[nodiscard]] const char *getTokBegin() const { return TokBegin; }
  [[nodiscard]] const char *getTokEnd() const { return TokEnd; }

  [[nodiscard]] std::unique_ptr<Token> take() { return std::move(Tok); }

  operator TokenView() const { return {Tok.get(), TokBegin, TokEnd}; }

  const Token *operator->() const { return Tok.get(); }
  operator const Token *() const { return Tok.get(); }
};

class Lexer {
  const std::string_view Src;
  DiagnosticEngine &Diags;
  const char *Cur;

  // token recorder
  const char *TokStartPtr;
  tok::TokenKind Tok;
  std::unique_ptr<Trivia> LeadingTrivia;
  void startToken() {
    Tok = tok::tok_unknown;
    TokStartPtr = Cur;
  }
  TokenAbs finishToken() {
    auto TokBody = std::make_unique<Token>(Tok, std::string(tokStr()),
                                           std::move(LeadingTrivia), nullptr);

    return {std::move(TokBody), TokStartPtr, Cur};
  }

  Trivia consumeTrivia();

  std::optional<TriviaPiece> tryConsumeWhitespaces();
  std::optional<TriviaPiece> tryConsumeComments();

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

  TokenAbs lex();
  TokenAbs lexString();
  TokenAbs lexIndString();
  TokenAbs lexPath();
};

} // namespace nixf
