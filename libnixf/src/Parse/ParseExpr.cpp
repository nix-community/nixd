#include "ParserImpl.h"

#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Basic/Nodes/Lambda.h"

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
  case tok_kw_if:
    return parseExprIf();
  default:
    break;
  }

  return parseExprOp();
}

std::unique_ptr<ExprIf> Parser::parseExprIf() {
  LexerCursor LCur = lCur(); // if
  Token TokIf = peek();
  assert(TokIf.kind() == tok_kw_if && "parseExprIf should start with `if`");
  consume(); // if
  assert(LastToken && "LastToken should be set after consume()");
  auto Cond = parseExpr();
  if (!Cond) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "condition");
    D.fix("remove `if` keyword").edit(TextEdit::mkRemoval(TokIf.range()));
    D.fix("insert dummy condition")
        .edit(TextEdit::mkInsertion(TokIf.rCur(), "true"));

    if (peek().kind() != tok_kw_then)
      return std::make_unique<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Cond), /*Then=*/nullptr,
                                      /*Else=*/nullptr);
  }

  Token TokThen = peek();
  if (TokThen.kind() != tok_kw_then) {
    Diagnostic &D = Diags.emplace_back(Diagnostic::DK_Expected,
                                       LexerCursorRange{LastToken->rCur()});
    D << std::string(tok::spelling(tok_kw_then));
    D.fix("insert `then` keyword")
        .edit(TextEdit::mkInsertion(TokThen.lCur(), " then"));
    Note &N = D.note(Note::NK_ToMachThis, TokIf.range());
    N << std::string(tok::spelling(tok_kw_if));

    return std::make_unique<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(Cond), /*Then=*/nullptr,
                                    /*Else=*/nullptr);
  }

  consume(); // then

  auto Then = parseExpr();
  if (!Then) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "then");
    Note &N = D.note(Note::NK_ToMachThis, TokIf.range());
    N << std::string(tok::spelling(tok_kw_if));

    if (peek().kind() != tok_kw_else)
      return std::make_unique<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Cond), std::move(Then),
                                      /*Else=*/nullptr);
  }

  Token TokElse = peek();
  if (TokElse.kind() != tok_kw_else) {
    Diagnostic &D =
        Diags.emplace_back(Diagnostic::DK_Expected, TokElse.range());
    D << std::string(tok::spelling(tok_kw_else));
    D.fix("insert `else` keyword")
        .edit(TextEdit::mkInsertion(TokElse.lCur(), " else"));

    return std::make_unique<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(Cond), std::move(Then),
                                    /*Else=*/nullptr);
  }

  consume(); // else

  auto Else = parseExpr();
  if (!Else) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "else");
    Note &N = D.note(Note::NK_ToMachThis, TokIf.range());
    N << std::string(tok::spelling(tok_kw_if));
  }

  return std::make_unique<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                  std::move(Cond), std::move(Then),
                                  std::move(Else));
}

} // namespace nixf
