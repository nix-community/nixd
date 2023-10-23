#pragma once

#include "nixf/Syntax/Token.h"
#include "nixf/Syntax/Trivia.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string_view>

namespace nixf {

class DiagnosticEngine;

class Lexer {
  const std::string_view Src;
  DiagnosticEngine &Diags;
  unsigned OffsetBase;
  const char *Cur;

  // token recorder
  const char *TokStartPtr;
  tok::TokenKind Tok;
  std::unique_ptr<Trivia> LeadingTrivia;
  void startToken() { TokStartPtr = Cur; }
  std::shared_ptr<Token> finishToken() {
    return std::make_shared<Token>(Tok, std::string(tokStr()),
                                   std::move(LeadingTrivia), nullptr);
  }

  Trivia consumeTrivia();

  std::optional<TriviaPiece> tryConsumeWhitespaces();
  std::optional<TriviaPiece> tryConsumeComments();

  unsigned curOffset() { return Cur - Src.begin() + OffsetBase; }

  bool eof(const char *Ptr) { return Ptr >= Src.end(); }

  bool eof() { return eof(Cur); }

  bool consumeEOL() { return consumePrefix("\r\n") || consumePrefix("\n"); }

  bool lexFloatExp();

  // Advance cursor if it starts with prefix, otherwise do nothing
  bool consumePrefix(std::string_view Prefix);

  /// Look ahead and check if we has \p Prefix
  bool prefix(std::string_view Prefix);

  void lexNumbers();

  [[nodiscard]] std::string_view tokStr() const { return {TokStartPtr, Cur}; }

  [[nodiscard]] std::string_view remain() const { return {Cur, Src.end()}; }

public:
  Lexer(std::string_view Src, DiagnosticEngine &Diags, unsigned OffsetBase = 0)
      : Src(Src), Diags(Diags), OffsetBase(OffsetBase) {
    Cur = Src.begin();
  }

  const char *cur() { return Cur; }

  /// Reset the cursor at source \p offset (zero-based indexing)
  void setCur(const char *NewCur) {
    assert(NewCur < Src.end());
    Cur = NewCur;
  }

  std::shared_ptr<Token> lex();
  std::shared_ptr<Token> lexString();
};

} // namespace nixf
