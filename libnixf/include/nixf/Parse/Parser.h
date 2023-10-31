#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Token.h"

#include <limits>
#include <map>
#include <queue>

namespace nixf {

using GuardTokensTy = std::map<tok::TokenKind, std::size_t>;
struct GuardTokensRAII {
  GuardTokensRAII(GuardTokensTy &GuardTokens, tok::TokenKind Kind)
      : Kind(Kind), GuardTokens(GuardTokens) {
    ++GuardTokens[Kind];
  }

  ~GuardTokensRAII() {
    assert(GuardTokens[Kind] > 0);
    --GuardTokens[Kind];
  }

  tok::TokenKind Kind;
  GuardTokensTy &GuardTokens;
};

class ExprSyntax;
class Parser {
  std::string_view Src;
  Lexer Lex;
  RawTwineBuilder Builder;
  DiagnosticEngine &Diag;

  /// These tokens should not be consumed as unknown nodes from error recovery.
  friend struct GuardTokensRAII;
  GuardTokensTy GuardTokens;

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

  void resetCur(const char *NewCur) {
    Lex.setCur(NewCur);
    LookAheadBuf.clear();
  }

  std::size_t getOffset(const char *Cur) { return Cur - Src.begin(); }

  // Private utilities.
  void matchBracket(tok::TokenKind LeftKind,
                    std::shared_ptr<RawNode> (Parser::*InnerParse)(),
                    tok::TokenKind RightKind);

  void addExprWithCheck(std::string As) {
    return addExprWithCheck(std::move(As), parseExpr());
  }

  /// Check if \p Expr is nullptr.
  /// Emit diagnostic if so.
  void addExprWithCheck(std::string As, std::shared_ptr<RawNode> Expr);

  // Concret n-terms.
  std::shared_ptr<RawNode> parseInterpolation();

  std::shared_ptr<RawNode> parseStringParts();
  std::shared_ptr<RawNode> parseString();

  std::shared_ptr<RawNode> parseIndString();
  std::shared_ptr<RawNode> parseIndStringParts();

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

  std::shared_ptr<RawNode> parseFormal();
  std::shared_ptr<RawNode> parseFormals();
  std::shared_ptr<RawNode> parseBracedFormals();
  std::shared_ptr<RawNode> parseLambdaArg();
  std::shared_ptr<RawNode> parseLambdaExpr();

  std::shared_ptr<RawNode> parseIfExpr();
  std::shared_ptr<RawNode> parseAssertExpr();
  std::shared_ptr<RawNode> parseWithExpr();
  std::shared_ptr<RawNode> parseLetInExpr();

  // Abstract level, these functions may return nullptr.
  std::shared_ptr<RawNode> parseExprSelect();
  std::shared_ptr<RawNode> parseExprSimple();

  /// Parse expr_app
  /// \p Limit of expr_select should be consumed, default to +inf
  std::shared_ptr<RawNode>
  parseExprApp(unsigned Limit = std::numeric_limits<unsigned>::max());

  std::shared_ptr<RawNode> parseExprOp() { return parseExprOpBP(0); }

  /// \note Pratt Parser.
  /// Pratt, Vaughan. "Top down operator precedence."
  /// Proceedings of the 1st Annual ACM SIGACT-SIGPLAN Symposium on Principles
  /// of Programming Languages (1973).
  /// https://web.archive.org/web/20151223215421/http://hall.org.ua/halls/wizzard/pdf/Vaughan.Pratt.TDOP.pdf
  /// \returns expr_op
  std::shared_ptr<RawNode> parseExprOpBP(unsigned LeftRBP);

  std::shared_ptr<RawNode> parseExpr();

  /// Create an "Unknown" with many tokens until \p Predicate does not hold
  std::shared_ptr<RawNode> parseUnknownUntilGuard();

public:
  explicit Parser(std::string_view Src, DiagnosticEngine &Diag)
      : Src(Src), Lex(Src, Diag), Diag(Diag) {}

  /// \brief Top-level parsing.
  /// \returns a "ROOT" node
  /// printing this node will exactly get the source file.
  /// \note non-null
  std::shared_ptr<RawNode> parse();
};

} // namespace nixf
