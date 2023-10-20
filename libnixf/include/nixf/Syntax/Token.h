#pragma once

#include "Token.h"

#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Trivia.h"

#include <string>

namespace nixf {

enum class TokenKind {
  TK_eof,
  TK_int,
  TK_float,
  TK_err,
};

struct Token : RawSyntax {
  TokenKind Kind;
  std::string Content;
  Trivia LeadingTrivia;
  Trivia TrailingTrivia;

  Token() : RawSyntax(SyntaxKind::Token, {}) {}
};

} // namespace nixf
