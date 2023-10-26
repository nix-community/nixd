#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Syntax/Token.h"

#include <queue>

namespace nixf {

class ExprSyntax;
class Parser {
  std::string_view Src;
  Lexer Lex;
  RawTwineBuilder Builder;
  DiagnosticEngine &Diag;

  std::deque<TokenView> LookAheadBuf;

  std::optional<TokenView> LastToken;

  const TokenView &peek(std::size_t N = 0,
                        TokenView (Lexer::*Ptr)() = &Lexer::lex) {
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

  void consumeOnly() {
    LastToken = LookAheadBuf.front();
    LookAheadBuf.pop_front();
  }

  std::size_t getOffset(const char *Cur) { return Cur - Src.begin(); }

  // Private utilities.
  void matchBracket(tok::TokenKind LeftKind,
                    std::shared_ptr<RawNode> (Parser::*InnerParse)(),
                    tok::TokenKind RightKind);

  // Concret n-terms.
  std::shared_ptr<RawNode> parseInterpolation();
  std::shared_ptr<RawNode> parseString();
  std::shared_ptr<RawNode> parsePath();
  std::shared_ptr<RawNode> parseAttrPath();
  std::shared_ptr<RawNode> parseAttrName();
  std::shared_ptr<RawNode> parseBinding();
  std::shared_ptr<RawNode> parseInherit();
  std::shared_ptr<RawNode> parseBinds();
  std::shared_ptr<RawNode> parseAttrSetExpr();
  std::shared_ptr<RawNode> parseParenExpr();
  std::shared_ptr<RawNode> parseLegacyLet();
  std::shared_ptr<RawNode> parseListExpr();
  std::shared_ptr<RawNode> parseListBody();

  // Abstract level.
  std::shared_ptr<RawNode> parseExprSelect();
  std::shared_ptr<RawNode> parseExprSimple();
  std::shared_ptr<RawNode> parseExprApp();

public:
  explicit Parser(std::string_view Src, DiagnosticEngine &Diag)
      : Src(Src), Lex(Src, Diag), Diag(Diag) {}
  std::shared_ptr<RawNode> parseExpr();
};

} // namespace nixf
