#include "nixf/Lex/Lexer.h"

#include <queue>

namespace nixf {

class ExprSyntax;
class Parser {
  Lexer &Lex;

public:
  explicit Parser(Lexer &Lex) : Lex(Lex) {}
  std::shared_ptr<ExprSyntax> parseString();
  std::shared_ptr<ExprSyntax> parseExpr();
  std::shared_ptr<ExprSyntax> parseSimple();
};

} // namespace nixf
