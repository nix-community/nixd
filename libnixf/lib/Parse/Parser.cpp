#include "nixf/Parse/Parser.h"
#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/Token.h"

namespace nixf {

using namespace tok;

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
std::shared_ptr<ExprSyntax> Parser::parseSimple() {
  std::shared_ptr<Token> Tok = Lex.lex();
  switch (Tok->Kind) {
  case tok_int:
    return std::make_shared<IntSyntax>(Tok);
  case tok_float:
    return std::make_shared<FloatSyntax>(Tok);
  }
}
} // namespace nixf
