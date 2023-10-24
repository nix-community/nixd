#include "nixf/Lex/Lexer.h"

#include <queue>

namespace nixf {

class ExprSyntax;
class Parser {
  Lexer &Lex;
  RawTwineBuilder Builder;

  std::deque<TokenView> LookAheadBuf;

  TokenView peek(std::size_t N = 0, TokenView (Lexer::*Ptr)() = &Lexer::lex) {
    while (N >= LookAheadBuf.size()) {
      LookAheadBuf.emplace_back((Lex.*Ptr)());
    }
    return LookAheadBuf[N];
  }

  void consume() {
    Builder.push(LookAheadBuf.front().get());
    consumeOnly();
  }

  void consumeOnly() { LookAheadBuf.pop_front(); }

  // Concret n-terms.
  std::shared_ptr<RawNode> parseInterpolation();
  std::shared_ptr<RawNode> parseString();

  // Abstract level.
  std::shared_ptr<RawNode> parseExprSimple();

public:
  explicit Parser(Lexer &Lex) : Lex(Lex) {}
  std::shared_ptr<RawNode> parseExpr();
};

} // namespace nixf
