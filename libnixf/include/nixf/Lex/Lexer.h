#pragma once

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Syntax/Token.h"

#include <optional>
#include <string_view>

namespace nixf {

class Lexer {
  const std::string_view Src;
  DiagnosticEngine &Diags;
  unsigned OffsetBase;
  const char *Cur;

  Trivia consumeTrivia();

  std::optional<TriviaPiece> tryConsumeWhitespaces();
  std::optional<TriviaPiece> tryConsumeComments();

  unsigned curOffset() { return Cur - Src.begin() + OffsetBase; }

  bool eof(const char *Ptr) { return Ptr >= Src.end(); }

  bool eof() { return eof(Cur); }

  bool tryAdvanceEOL();

  bool lexFloatExp(std::string &NumStr);

  [[nodiscard]] std::string_view remain() const { return {Cur, Src.end()}; }

public:
  Lexer(std::string_view Src, DiagnosticEngine &Diags, unsigned OffsetBase = 0)
      : Src(Src), Diags(Diags), OffsetBase(OffsetBase) {
    Cur = Src.begin();
  }

  std::shared_ptr<Token> lex();
};

} // namespace nixf
