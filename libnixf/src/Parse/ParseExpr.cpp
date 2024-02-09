#include "Parser.h"

namespace nixf {

using namespace detail;

std::unique_ptr<Expr> Parser::parseExprSelect() {
  std::unique_ptr<Expr> Expr = parseExprSimple();
  if (!Expr)
    return nullptr;
  assert(LastToken && "LastToken should be set after valid expr");
  LexerCursor Begin = Expr->lCur();

  Token Tok = peek();
  if (Tok.kind() != tok_dot) {
    // expr_select : expr_simple
    return Expr;
  }

  // expr_select : expr_simple '.' attrpath
  //             | expr_simple '.' attrpath 'or' expr_select
  consume(); // .
  auto Path = parseAttrPath();
  if (!Path) {
    // extra ".", consider remove it.
    Diagnostic &D =
        Diags.emplace_back(Diagnostic::DK_SelectExtraDot, Tok.range());
    D.fix("remove extra .").edit(TextEdit::mkRemoval(Tok.range()));
    D.fix("insert dummy attrpath")
        .edit(TextEdit::mkInsertion(Tok.rCur(), R"("dummy")"));
  }

  Token TokOr = peek();
  if (TokOr.kind() != tok_kw_or) {
    // expr_select : expr_simple '.' attrpath
    return std::make_unique<ExprSelect>(
        LexerCursorRange{Begin, LastToken->rCur()}, std::move(Expr),
        std::move(Path), /*Default=*/nullptr);
  }

  // expr_select : expr_simple '.' attrpath 'or' expr_select
  consume(); // `or`
  auto Default = parseExprSelect();
  if (!Default) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "default");
    D.fix("remove `or` keyword").edit(TextEdit::mkRemoval(TokOr.range()));
  }
  return std::make_unique<ExprSelect>(
      LexerCursorRange{Begin, LastToken->rCur()}, std::move(Expr),
      std::move(Path), std::move(Default));
}

std::unique_ptr<Expr> Parser::parseExprApp(int Limit) {
  std::unique_ptr<Expr> Fn = parseExprSelect();
  // If fn cannot be evaluated to lambda, exit early.
  if (!Fn || !Fn->maybeLambda())
    return Fn;

  std::vector<std::unique_ptr<Expr>> Args;
  while (Limit--) {
    std::unique_ptr<Expr> Arg = parseExprSelect();
    if (!Arg)
      break;
    Args.emplace_back(std::move(Arg));
  }

  if (Args.empty())
    return Fn;
  return std::make_unique<ExprCall>(
      LexerCursorRange{Fn->lCur(), Args.back()->rCur()}, std::move(Fn),
      std::move(Args));
}

std::unique_ptr<Expr> Parser::parseExpr() {
  // Look ahead 3 tokens.
  switch (peek().kind()) {
  case tok_id: {
    switch (peek(1).kind()) {
    case tok_at:    // ID @
    case tok_colon: // ID :
      return parseExprLambda();
    default:
      break;
    }
    break;
  }
  case tok_l_curly: {
    switch (peek(1).kind()) {
    case tok_id: {
      switch (peek(2).kind()) {
      case tok_colon:    // { a :
      case tok_at:       // { a @
      case tok_question: // { a ?
      case tok_comma:    // { a ,
      case tok_id:       // { a b
      case tok_ellipsis: // { a ...
        return parseExprLambda();
      default:
        break;
      }
      break;
    }
    default:
      break;
    }
    break;
  } // case tok_l_curly
  default:
    break;
  }

  return parseExprOp();
}

} // namespace nixf
