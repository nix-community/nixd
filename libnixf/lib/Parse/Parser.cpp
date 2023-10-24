#include "nixf/Parse/Parser.h"
#include "nixf/Syntax/RawSyntax.h"
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
std::shared_ptr<RawNode> Parser::parseSimple() {
  std::shared_ptr<Token> Tok = Lex.lex();
  switch (Tok->getKind()) {
  case tok_int:
  case tok_float:
    return Tok;
  }
}
} // namespace nixf
