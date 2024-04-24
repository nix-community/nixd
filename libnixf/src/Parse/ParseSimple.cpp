#include "Parser.h"

#include <charconv>

using namespace nixf;
using namespace nixf::detail;

namespace {

/// \brief  Whether the node could be produced by non-term \p expr_simple
bool mayProducedBySimple(Node::NodeKind NK) {
  switch (NK) {
  case Node::NK_ExprVar:
  case Node::NK_ExprInt:
  case Node::NK_ExprFloat:
  case Node::NK_ExprSPath:
  case Node::NK_ExprString:
  case Node::NK_ExprPath:
  case Node::NK_ExprParen:
  case Node::NK_ExprAttrs:
  case Node::NK_ExprList:
    return true;
  default:
    break;
  }
  return false;
}

bool mayProducedBySimple(const Expr *E) {
  if (!E)
    return false;
  return mayProducedBySimple(E->kind());
}

} // namespace

std::shared_ptr<ExprParen> Parser::parseExprParen() {
  Token L = peek();
  auto LParen = std::make_shared<Misc>(L.range());
  assert(L.kind() == tok_l_paren);
  consume(); // (
  auto Sync = withSync(tok_r_paren);
  assert(LastToken && "LastToken should be set after consume()");
  auto Expr = parseExpr();
  if (!Expr)
    diagNullExpr(Diags, LastToken->rCur(), "parenthesized");
  if (ExpectResult ER = expect(tok_r_paren); ER.ok()) {
    consume(); // )
    auto RParen = std::make_shared<Misc>(ER.tok().range());
    if (mayProducedBySimple(Expr.get())) {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_RedundantParen, LParen->range());
      D.tag(DiagnosticTag::Faded);
      Fix &F = D.fix("remove ( and )");
      F.edit(TextEdit::mkRemoval(LParen->range()));
      F.edit(TextEdit::mkRemoval(RParen->range()));
    }
    return std::make_shared<ExprParen>(
        LexerCursorRange{L.lCur(), ER.tok().rCur()}, std::move(Expr),
        std::move(LParen), std::move(RParen));
  } else { // NOLINT(readability-else-after-return)
    ER.diag().note(Note::NK_ToMachThis, L.range())
        << std::string(tok::spelling(tok_l_paren));
    if (mayProducedBySimple(Expr.get())) {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_RedundantParen, LParen->range());
      D.tag(DiagnosticTag::Faded);
      Fix &F = D.fix("remove (");
      F.edit(TextEdit::mkRemoval(LParen->range()));
    }
    return std::make_shared<ExprParen>(
        LexerCursorRange{L.lCur(), LastToken->rCur()}, std::move(Expr),
        std::move(LParen),
        /*RParen=*/nullptr);
  }
}

std::shared_ptr<ExprList> Parser::parseExprList() {
  Token Tok = peek();
  if (Tok.kind() != tok_l_bracket)
    return nullptr;
  consume(); // [
  auto Sync = withSync(tok_r_bracket);
  assert(LastToken && "LastToken should be set after consume()");
  LexerCursor Begin = Tok.lCur();
  std::vector<std::shared_ptr<Expr>> Exprs;
  while (true) {
    if (Token Tok = peek(); Tok.kind() == tok_r_bracket)
      break;
    std::shared_ptr<Expr> Expr = parseExprSelect();
    if (!Expr)
      break;
    Exprs.emplace_back(std::move(Expr));
  }
  if (ExpectResult ER = expect(tok_r_bracket); ER.ok())
    consume();
  else
    ER.diag().note(Note::NK_ToMachThis, Tok.range())
        << std::string(tok::spelling(tok_l_bracket));
  return std::make_shared<ExprList>(LexerCursorRange{Begin, LastToken->rCur()},
                                    std::move(Exprs));
}

std::shared_ptr<Expr> Parser::parseExprSimple() {
  Token Tok = peek();
  switch (Tok.kind()) {
  case tok_id: {
    consume();
    auto ID =
        std::make_shared<Identifier>(Tok.range(), std::string(Tok.view()));
    return std::make_shared<ExprVar>(Tok.range(), std::move(ID));
  }
  case tok_int: {
    consume();
    NixInt N = 0;
    auto [Ptr, Errc] = std::from_chars(Tok.view().begin(), Tok.view().end(), N);
    if (Errc != std::errc()) {
      // Cannot decode int from tok_int.
      assert(Errc == std::errc::result_out_of_range);
      // emit a diagnostic saying we cannot decode integer to NixInt.
      Diags.emplace_back(Diagnostic::DK_IntTooBig, Tok.range());
    }
    return std::make_shared<ExprInt>(Tok.range(), N);
  }
  case tok_float: {
    consume();
    // libc++ doesn't support std::from_chars for floating point numbers.
    NixFloat N = std::strtof(std::string(Tok.view()).c_str(), nullptr);
    return std::make_shared<ExprFloat>(Tok.range(), N);
  }
  case tok_spath: {
    consume();
    return std::make_shared<ExprSPath>(
        Tok.range(), std::string(Tok.view().substr(1, Tok.view().size() - 2)));
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
