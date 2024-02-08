#include "Parser.h"

#include <charconv>

namespace nixf {

using namespace detail;

std::unique_ptr<ExprParen> Parser::parseExprParen() {
  Token L = peek();
  auto LParen = std::make_unique<Misc>(L.range());
  assert(L.kind() == tok_l_paren);
  consume(); // (
  auto Sync = withSync(tok_r_paren);
  assert(LastToken && "LastToken should be set after consume()");
  auto Expr = parseExpr();
  if (!Expr)
    diagNullExpr(Diags, LastToken->rCur(), "parenthesized");
  if (ExpectResult ER = expect(tok_r_paren); ER.ok()) {
    consume(); // )
    auto RParen = std::make_unique<Misc>(ER.tok().range());
    return std::make_unique<ExprParen>(
        LexerCursorRange{L.lCur(), ER.tok().rCur()}, std::move(Expr),
        std::move(LParen), std::move(RParen));
  } else { // NOLINT(readability-else-after-return)
    ER.diag().note(Note::NK_ToMachThis, L.range())
        << std::string(tok::spelling(tok_l_paren));
    return std::make_unique<ExprParen>(
        LexerCursorRange{L.lCur(), LastToken->rCur()}, std::move(Expr),
        std::move(LParen),
        /*RParen=*/nullptr);
  }
}

std::unique_ptr<ExprList> Parser::parseExprList() {
  Token Tok = peek();
  if (Tok.kind() != tok_l_bracket)
    return nullptr;
  consume(); // [
  auto Sync = withSync(tok_r_bracket);
  assert(LastToken && "LastToken should be set after consume()");
  LexerCursor Begin = Tok.lCur();
  std::vector<std::unique_ptr<Expr>> Exprs;
  while (true) {
    if (Token Tok = peek(); Tok.kind() == tok_r_bracket)
      break;
    std::unique_ptr<Expr> Expr = parseExprSelect();
    if (!Expr)
      break;
    Exprs.emplace_back(std::move(Expr));
  }
  if (ExpectResult ER = expect(tok_r_bracket); ER.ok())
    consume();
  else
    ER.diag().note(Note::NK_ToMachThis, Tok.range())
        << std::string(tok::spelling(tok_l_bracket));
  return std::make_unique<ExprList>(LexerCursorRange{Begin, LastToken->rCur()},
                                    std::move(Exprs));
}

std::unique_ptr<Expr> Parser::parseExprSimple() {
  Token Tok = peek();
  switch (Tok.kind()) {
  case tok_id: {
    consume();
    auto ID =
        std::make_unique<Identifier>(Tok.range(), std::string(Tok.view()));
    return std::make_unique<ExprVar>(Tok.range(), std::move(ID));
  }
  case tok_int: {
    consume();
    NixInt N;
    std::from_chars_result Result [[maybe_unused]] =
        std::from_chars(Tok.view().begin(), Tok.view().end(), N);
    assert(Result.ec == std::errc() && "should be a valid integer");
    return std::make_unique<ExprInt>(Tok.range(), N);
  }
  case tok_float: {
    consume();
    // libc++ doesn't support std::from_chars for floating point numbers.
    NixFloat N = std::strtof(std::string(Tok.view()).c_str(), nullptr);
    return std::make_unique<ExprFloat>(Tok.range(), N);
  }
  case tok_dquote: // "  - normal strings
    return parseString(/*IsIndented=*/false);
  case tok_quote2: // '' - indented strings
    return parseString(/*IsIndented=*/true);
  case tok_path_fragment:
    return parseExprPath();
  case tok_l_paren:
    return parseExprParen();
  case tok_kw_rec:
  case tok_l_curly:
    return parseExprAttrs();
  case tok_l_bracket:
    return parseExprList();
  default:
    return nullptr;
  }
}

} // namespace nixf
