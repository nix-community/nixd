#include "Parser.h"

using namespace nixf;
using namespace nixf::detail;

std::shared_ptr<Expr> Parser::parseExprSelect() {
  std::shared_ptr<Expr> Expr = parseExprSimple();
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
  auto Do = std::make_shared<Dot>(Tok.range(), Expr.get(), nullptr);
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
    return std::make_shared<ExprSelect>(
        LexerCursorRange{Begin, LastToken->rCur()}, std::move(Expr),
        std::move(Do), std::move(Path), /*Default=*/nullptr,
        /*DesugaredFrom=*/nullptr);
  }

  // expr_select : expr_simple '.' attrpath 'or' expr_select
  consume(); // `or`
  auto Default = parseExprSelect();
  if (!Default) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "default");
    D.fix("remove `or` keyword").edit(TextEdit::mkRemoval(TokOr.range()));
  }
  return std::make_shared<ExprSelect>(
      LexerCursorRange{Begin, LastToken->rCur()}, std::move(Expr),
      std::move(Do), std::move(Path), std::move(Default),
      /*DesugaredFrom=*/nullptr);
}

std::shared_ptr<Expr> Parser::parseExprApp(int Limit) {
  std::shared_ptr<Expr> Fn = parseExprSelect();
  // If fn cannot be evaluated to lambda, exit early.
  if (!Fn || !Fn->maybeLambda())
    return Fn;

  std::vector<std::shared_ptr<Expr>> Args;
  while (Limit--) {
    std::shared_ptr<Expr> Arg = parseExprSelect();
    if (!Arg)
      break;
    Args.emplace_back(std::move(Arg));
  }

  if (Args.empty())
    return Fn;
  return std::make_shared<ExprCall>(
      LexerCursorRange{Fn->lCur(), Args.back()->rCur()}, std::move(Fn),
      std::move(Args));
}

std::shared_ptr<Expr> Parser::parseExpr() {
  // Look ahead 4 tokens.
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
      case tok_r_curly:
        switch (peek(3).kind()) {
        case tok_colon: // { a } :
        case tok_at:    // { a } @
          return parseExprLambda();
        default:
          return parseExprAttrs();
        }
      default:
        break;
      }
      break;
    }
    case tok_r_curly:
      switch (peek(2).kind()) {
      case tok_at:    // { } @
      case tok_colon: // { } :
        return parseExprLambda();
      default:
        break;
      }
      break;
    case tok_ellipsis: // { ...
      return parseExprLambda();
    default:
      break;
    }
    break;
  } // case tok_l_curly
  case tok_kw_if:
    return parseExprIf();
  case tok_kw_assert:
    return parseExprAssert();
  case tok_kw_let:
    if (peek(1).kind() != tok_l_curly)
      return parseExprLet();
    break;
  case tok_kw_with:
    return parseExprWith();
  default:
    break;
  }

  return parseExprOp();
}

std::shared_ptr<ExprIf> Parser::parseExprIf() {
  LexerCursor LCur = lCur(); // if
  Token TokIf = peek();
  assert(TokIf.kind() == tok_kw_if && "parseExprIf should start with `if`");
  consume(); // if
  assert(LastToken && "LastToken should be set after consume()");

  auto SyncThen = withSync(tok_kw_then);
  auto SyncElse = withSync(tok_kw_else);

  auto Cond = parseExpr();
  if (!Cond) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "condition");
    D.fix("remove `if` keyword").edit(TextEdit::mkRemoval(TokIf.range()));
    D.fix("insert dummy condition")
        .edit(TextEdit::mkInsertion(TokIf.rCur(), "true"));

    if (peek().kind() != tok_kw_then)
      return std::make_shared<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Cond), /*Then=*/nullptr,
                                      /*Else=*/nullptr);
  }

  ExpectResult ExpKwThen = expect(tok_kw_then);
  if (!ExpKwThen.ok()) {
    Diagnostic &D = ExpKwThen.diag();
    Note &N = D.note(Note::NK_ToMachThis, TokIf.range());
    N << std::string(tok::spelling(tok_kw_if));
    return std::make_shared<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
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
      return std::make_shared<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Cond), std::move(Then),
                                      /*Else=*/nullptr);
  }

  ExpectResult ExpKwElse = expect(tok_kw_else);
  if (!ExpKwElse.ok())
    return std::make_shared<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(Cond), std::move(Then),
                                    /*Else=*/nullptr);

  consume(); // else

  auto Else = parseExpr();
  if (!Else) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "else");
    Note &N = D.note(Note::NK_ToMachThis, TokIf.range());
    N << std::string(tok::spelling(tok_kw_if));
  }

  return std::make_shared<ExprIf>(LexerCursorRange{LCur, LastToken->rCur()},
                                  std::move(Cond), std::move(Then),
                                  std::move(Else));
}

std::shared_ptr<ExprAssert> Parser::parseExprAssert() {
  LexerCursor LCur = lCur();
  Token TokAssert = peek();
  assert(TokAssert.kind() == tok_kw_assert && "should be tok_kw_assert");
  consume(); // assert
  assert(LastToken && "LastToken should be set after consume()");

  auto SyncSemi = withSync(tok_semi_colon);

  auto Cond = parseExpr();
  if (!Cond) {
    Diagnostic &D = diagNullExpr(Diags, LastToken->rCur(), "condition");
    D.fix("remove `assert` keyword")
        .edit(TextEdit::mkRemoval(TokAssert.range()));

    if (peek().kind() != tok_colon)
      return std::make_shared<ExprAssert>(
          LexerCursorRange{LCur, LastToken->rCur()}, std::move(Cond),
          /*Value=*/nullptr);
  }

  ExpectResult ExpSemi = expect(tok_semi_colon);
  if (!ExpSemi.ok()) {
    // missing ';'
    Diagnostic &D = ExpSemi.diag();
    Note &N = D.note(Note::NK_ToMachThis, TokAssert.range());
    N << std::string(tok::spelling(tok_kw_assert));
    return std::make_shared<ExprAssert>(
        LexerCursorRange{LCur, LastToken->rCur()}, std::move(Cond),
        /*Value=*/nullptr);
  }

  consume(); // ;

  auto Value = parseExpr();

  if (!Value)
    diagNullExpr(Diags, LastToken->rCur(), "assert value");

  return std::make_shared<ExprAssert>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Cond), std::move(Value));
}

std::shared_ptr<ExprLet> Parser::parseExprLet() {
  LexerCursor LCur = lCur();
  Token TokLet = peek();
  assert(TokLet.kind() == tok_kw_let &&
         "first token should be tok_kw_let in parseExprLet()");

  auto Let = std::make_shared<Misc>(TokLet.range());

  consume(); // 'let'

  auto SyncIn = withSync(tok_kw_in);

  assert(LastToken && "LastToken should be set after consume()");

  auto Binds = parseBinds();
  auto Attrs = Binds ? Act.onExprAttrs(Binds->range(), Binds, Let) : nullptr;

  ExpectResult ExpKwIn = expect(tok_kw_in);

  if (!ExpKwIn.ok()) {
    // missing 'in'
    return std::make_shared<ExprLet>(LexerCursorRange{LCur, LastToken->rCur()},
                                     std::move(Let), /*KwIn=*/nullptr,
                                     /*E=*/nullptr, std::move(Attrs));
  }

  auto In = std::make_shared<Misc>(ExpKwIn.tok().range());

  consume(); // 'in'

  auto E = parseExpr();
  if (!E)
    diagNullExpr(Diags, LastToken->rCur(), "let ... in");

  return std::make_shared<ExprLet>(LexerCursorRange{LCur, LastToken->rCur()},
                                   std::move(Let), std::move(In), std::move(E),
                                   std::move(Attrs));
}

std::shared_ptr<ExprWith> Parser::parseExprWith() {
  LexerCursor LCur = lCur();
  Token TokWith = peek();
  assert(TokWith.kind() == tok_kw_with && "token should be tok_kw_with");

  consume(); // with

  auto KwWith = std::make_shared<Misc>(TokWith.range());
  assert(LastToken && "LastToken should be set after consume()");

  auto SyncSemi = withSync(tok_semi_colon);

  auto With = parseExpr();

  if (!With)
    diagNullExpr(Diags, LastToken->rCur(), "with expression");

  ExpectResult ExpSemi = expect(tok_semi_colon);
  if (!ExpSemi.ok()) {
    ExpSemi.diag().note(Note::NK_ToMachThis, TokWith.range())
        << std::string(tok::spelling(tok_kw_with));
    return std::make_shared<ExprWith>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(KwWith), /*TokSemi*/ nullptr,
                                      std::move(With), /*E=*/nullptr);
  }

  auto TokSemi = std::make_shared<Misc>(ExpSemi.tok().range());
  consume(); // ;

  auto E = parseExpr();

  if (!E)
    diagNullExpr(Diags, LastToken->rCur(), "with body");

  return std::make_shared<ExprWith>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(KwWith), std::move(TokSemi),
                                    std::move(With), std::move(E));
}
