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

  std::deque<TokenAbs> LookAheadBuf;

  std::optional<TokenView> LastToken;

  TokenView peek(std::size_t N = 0, TokenAbs (Lexer::*Ptr)() = &Lexer::lex) {
    while (N >= LookAheadBuf.size()) {
      LookAheadBuf.emplace_back((Lex.*Ptr)());
    }
    return LookAheadBuf[N];
  }

  void consume() {
    if (LookAheadBuf.empty())
      peek(0);
    Builder.push(LookAheadBuf.front().take());
    popFront();
  }

  std::unique_ptr<Token> popFront() {
    LastToken = LookAheadBuf.front();
    std::unique_ptr<Token> Ptr = LookAheadBuf.front().take();
    LookAheadBuf.pop_front();
    return Ptr;
  }

  void resetCur(const char *NewCur) {
    Lex.setCur(NewCur);
    LookAheadBuf.clear();
  }

  std::size_t getOffset(const char *Cur) { return Cur - Src.begin(); }

  // Private utilities.
  void matchBracket(tok::TokenKind LeftKind,
                    std::unique_ptr<RawNode> (Parser::*InnerParse)(),
                    tok::TokenKind RightKind);

  void addExprWithCheck(std::string As) {
    return addExprWithCheck(std::move(As), parseExpr());
  }

  /// Check if \p Expr is nullptr.
  /// Emit diagnostic if so.
  void addExprWithCheck(std::string As, std::unique_ptr<RawNode> Expr);

  // Concret n-terms.
  std::unique_ptr<RawNode> parseInterpolation();

  std::unique_ptr<RawNode> parseStringParts();
  std::unique_ptr<RawNode> parseString();

  std::unique_ptr<RawNode> parseIndString();
  std::unique_ptr<RawNode> parseIndStringParts();

  std::unique_ptr<RawNode> parsePath();
  std::unique_ptr<RawNode> parseAttrPath();
  std::unique_ptr<RawNode> parseAttrName();
  std::unique_ptr<RawNode> parseBinding();
  std::unique_ptr<RawNode> parseInherit();
  std::unique_ptr<RawNode> parseBinds();
  std::unique_ptr<RawNode> parseAttrSetExpr();
  std::unique_ptr<RawNode> parseParenExpr();
  std::unique_ptr<RawNode> parseLegacyLet();
  std::unique_ptr<RawNode> parseListExpr();
  std::unique_ptr<RawNode> parseListBody();

  std::unique_ptr<RawNode> parseFormal();
  std::unique_ptr<RawNode> parseFormals();
  std::unique_ptr<RawNode> parseBracedFormals();
  std::unique_ptr<RawNode> parseLambdaArg();
  std::unique_ptr<RawNode> parseLambdaExpr();

  std::unique_ptr<RawNode> parseIfExpr();
  std::unique_ptr<RawNode> parseAssertExpr();
  std::unique_ptr<RawNode> parseWithExpr();
  std::unique_ptr<RawNode> parseLetInExpr();

  // Abstract level, these functions may return nullptr.
  std::unique_ptr<RawNode> parseExprSelect();
  std::unique_ptr<RawNode> parseExprSimple();

  /// Parse expr_app
  /// \p Limit of expr_select should be consumed, default to +inf
  std::unique_ptr<RawNode>
  parseExprApp(unsigned Limit = std::numeric_limits<unsigned>::max());

  std::unique_ptr<RawNode> parseExprOp() { return parseExprOpBP(0); }

  /// \note Pratt Parser.
  /// Pratt, Vaughan. "Top down operator precedence."
  /// Proceedings of the 1st Annual ACM SIGACT-SIGPLAN Symposium on Principles
  /// of Programming Languages (1973).
  /// https://web.archive.org/web/20151223215421/http://hall.org.ua/halls/wizzard/pdf/Vaughan.Pratt.TDOP.pdf
  /// \returns expr_op
  std::unique_ptr<RawNode> parseExprOpBP(unsigned LeftRBP);

  std::unique_ptr<RawNode> parseExpr();

  /// Create an "Unknown" with many tokens until \p Predicate does not hold
  std::unique_ptr<RawNode> parseUnknownUntilGuard();

public:
  explicit Parser(std::string_view Src, DiagnosticEngine &Diag)
      : Src(Src), Lex(Src, Diag), Diag(Diag) {}

  /// \brief Top-level parsing.
  /// \returns a "ROOT" node
  /// printing this node will exactly get the source file.
  /// \note non-null
  std::unique_ptr<RawNode> parse();
};

} // namespace nixf
