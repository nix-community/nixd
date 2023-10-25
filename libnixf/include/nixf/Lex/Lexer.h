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
  std::shared_ptr<Token> Tok;

  const char *TokBegin;
  const char *TokEnd;

public:
  TokenView(std::shared_ptr<Token> Tok, const char *TB, const char *TE)
      : Tok(std::move(Tok)), TokBegin(TB), TokEnd(TE) {}
  operator std::shared_ptr<Token>() const { return Tok; }

  [[nodiscard]] const char *getTokBegin() const { return TokBegin; }
  [[nodiscard]] const char *getTokEnd() const { return TokEnd; }

  [[nodiscard]] std::shared_ptr<Token> get() const { return Tok; }

  std::shared_ptr<Token> operator->() const { return Tok; }

  OffsetRange getTokRange(const char *Base) const {
    assert(Base <= TokBegin && TokBegin <= TokEnd);
    return {static_cast<size_t>(TokBegin - Base),
            static_cast<size_t>(TokEnd - Base)};
  }
};

class Lexer {
  const std::string_view Src;
  DiagnosticEngine &Diags;
  const char *Base;
  const char *Cur;

  // token recorder
  const char *TokStartPtr;
  tok::TokenKind Tok;
  std::unique_ptr<Trivia> LeadingTrivia;
  void startToken() { TokStartPtr = Cur; }
  TokenView finishToken() {
    auto TokBody = std::make_shared<Token>(Tok, std::string(tokStr()),
                                           std::move(LeadingTrivia), nullptr);

    return {std::move(TokBody), TokStartPtr, Cur};
  }

  Trivia consumeTrivia();

  std::optional<TriviaPiece> tryConsumeWhitespaces();
  std::optional<TriviaPiece> tryConsumeComments();

  std::size_t curOffset() { return Cur - Base; }

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

  /// Should be called after lexing a "raw" identifier, we check if it is a
  /// keyword and make assignment: `Tok <- tok_kw_*`
  void maybeKW();

  void lexIdentifier();

  void lexNumbers();

  [[nodiscard]] std::string_view tokStr() const { return {TokStartPtr, Cur}; }

  [[nodiscard]] std::string_view remain() const { return {Cur, Src.end()}; }

public:
  Lexer(std::string_view Src, DiagnosticEngine &Diags,
        const char *OffsetBase = nullptr)
      : Src(Src), Diags(Diags), Base(OffsetBase) {
    Cur = Src.begin();
    if (!Base)
      Base = Src.begin();
  }

  const char *cur() { return Cur; }

  /// Reset the cursor at source \p offset (zero-based indexing)
  void setCur(const char *NewCur) {
    assert(NewCur < Src.end());
    Cur = NewCur;
  }

  TokenView lex();
  TokenView lexString();
};

} // namespace nixf
