#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Syntax/Token.h"

#include <queue>

namespace nixf {

class ExprSyntax;
class Parser {
  Lexer Lex;
  RawTwineBuilder Builder;
  DiagnosticEngine &Diag;

  std::deque<TokenView> LookAheadBuf;

  TokenView peek(std::size_t N = 0, TokenView (Lexer::*Ptr)() = &Lexer::lex) {
    while (N >= LookAheadBuf.size()) {
      LookAheadBuf.emplace_back((Lex.*Ptr)());
    }
    return LookAheadBuf[N];
  }

  void consume() {
    if (LookAheadBuf.empty())
      peek(0);
    Builder.push(LookAheadBuf.front().get());
    consumeOnly();
  }

  void consumeOnly() { LookAheadBuf.pop_front(); }

  // Concret n-terms.
  std::shared_ptr<RawNode> parseInterpolation();
  std::shared_ptr<RawNode> parseString();
  std::shared_ptr<RawNode> parseAttrPath();
  std::shared_ptr<RawNode> parseAttrName();
  std::shared_ptr<RawNode> parseBinding();
  std::shared_ptr<RawNode> parseInherit();
  std::shared_ptr<RawNode> parseBinds();
  std::shared_ptr<RawNode> parseAttrSetExpr();

  // Abstract level.
  std::shared_ptr<RawNode> parseExprSimple();

public:
  explicit Parser(std::string_view Src, DiagnosticEngine &Diag)
      : Lex(Src, Diag), Diag(Diag) {}
  std::shared_ptr<RawNode> parseExpr();
};

} // namespace nixf
