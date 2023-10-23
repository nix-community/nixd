#pragma once

#include "Token.h"

#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Trivia.h"

#include <string>

namespace nixf {

namespace tok {

enum TokenKind {
  tok_eof,

  tok_int,
  tok_float,

  tok_err,
};

} // namespace tok

struct Token : RawSyntax {
  tok::TokenKind Kind;
  std::string Content;
  Trivia LeadingTrivia;
  Trivia TrailingTrivia;

  Token() : RawSyntax(SyntaxKind::Token, {}) {}
};

} // namespace nixf
