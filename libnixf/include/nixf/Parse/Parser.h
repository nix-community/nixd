#include "nixf/Lex/Lexer.h"

#include <queue>

namespace nixf {

class ExprSyntax;
class Parser {
  Lexer &Lex;

public:
  explicit Parser(Lexer &Lex) : Lex(Lex) {}
  std::shared_ptr<RawNode> parseString();
  std::shared_ptr<RawNode> parseExpr();
  std::shared_ptr<RawNode> parseSimple();
};

} // namespace nixf
