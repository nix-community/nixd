#include "nixf/Parse/Parser.h"
#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/Token.h"

namespace nixf {

using namespace tok;

/// interpolation : ${ expr }
std::shared_ptr<RawNode> Parser::parseInterpolation() {
  Builder.start(SyntaxKind::SK_Interpolation);
  consume();
  Builder.push(parseExpr());
  TokenView Tok = peek();
  if (Tok->getKind() != tok_r_curly) {
    // Error
  } else {
    consume();
  }
  return Builder.finsih();
}

// string: '"' {string_part} '"'
// string : '"' {string_part} '"'
// string_part : STRING_FRAGMENT | interpolation | STRING_ESCAPED
std::shared_ptr<RawNode> Parser::parseString() {
  Builder.start(SyntaxKind::SK_String);
  consume();
  while (true) {
    TokenView Tok = peek(0, &Lexer::lexString);
    bool Finish = false;
    switch (Tok->getKind()) {
    case tok_dquote:
      // end of a string.
      Finish = true;
      consume();
      break;
    case tok_eof:
      // encountered EOF, unbalanced dquote.
      Finish = true;
      break;
    case tok_dollar_curly: {
      // interpolation, we need to parse a subtree then.
      Builder.push(parseInterpolation());
      break;
    }
    case tok_string_part:
      // If this is a part of string, just push it.
      consume();
      break;
    }
    if (Finish)
      break;
  }
  return Builder.finsih();
}

std::shared_ptr<RawNode> Parser::parseExpr() { return parseExprSimple(); }

// simple :  INT
//        | FLOAT
//        | string
//        | indented_string
//        | path
//        | hpath
//        | uri
//        | '(' expr ')'
//        | legacy_let
//        | attr
//        | list
std::shared_ptr<RawNode> Parser::parseExprSimple() {
  TokenView Tok = peek();
  switch (Tok->getKind()) {
  case tok_int:
  case tok_float:
    consumeOnly();
    return Tok.get();
  case tok_dquote:
    return parseString();
  }
  return nullptr;
}
} // namespace nixf
