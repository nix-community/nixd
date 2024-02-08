#include "Parser.h"

namespace nixf {

using namespace detail;

std::unique_ptr<Formal> Parser::parseFormal() {
  // formal : ,? ID
  //        | ,? ID '?' expr
  //        | ,? ...

  LexerCursor LCur = lCur();
  std::unique_ptr<Misc> Comma = nullptr;
  if (Token Tok = peek(); Tok.kind() == tok_comma) {
    consume();
    Comma = std::make_unique<Misc>(Tok.range());
  }
  if (Token Tok = peek(); Tok.kind() == tok_id) {
    consume(); // ID
    assert(LastToken && "LastToken should be set after consume()");
    auto ID =
        std::make_unique<Identifier>(Tok.range(), std::string(Tok.view()));
    if (peek().kind() != tok_question)
      return std::make_unique<Formal>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Comma), std::move(ID), nullptr);
    consume(); // ?
    std::unique_ptr<Expr> Default = parseExpr();
    if (!Default)
      diagNullExpr(Diags, LastToken->rCur(), "default value");
    return std::make_unique<Formal>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(Comma), std::move(ID),
                                    std::move(Default));
  }
  if (Token Tok = peek(); Tok.kind() == tok_ellipsis) {
    consume(); // ...
    assert(LastToken && "LastToken should be set after consume()");
    std::unique_ptr<Misc> Ellipsis = std::make_unique<Misc>(Tok.range());
    return std::make_unique<Formal>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(Comma), std::move(Ellipsis));
  }

  if (Comma) {
    assert(LastToken && "LastToken should be set after consume()");
    return std::make_unique<Formal>(LexerCursorRange{LCur, LastToken->rCur()},
                                    std::move(Comma), /*ID=*/nullptr,
                                    /*Default=*/nullptr);
  }
  return nullptr;
}

std::unique_ptr<Formals> Parser::parseFormals() {
  ExpectResult ER = expect(tok_l_curly);
  if (!ER.ok())
    return nullptr;
  Token TokLCurly = ER.tok();
  consume(); // {
  assert(LastToken && "LastToken should be set after consume()");
  auto SyncRCurly = withSync(tok_r_curly);
  auto SyncComma = withSync(tok_comma);
  auto SyncQuestion = withSync(tok_question);
  auto SyncID = withSync(tok_id);
  LexerCursor LCur = ER.tok().lCur();
  std::vector<std::unique_ptr<Formal>> Members;
  while (true) {
    if (Token Tok = peek(); Tok.kind() == tok_r_curly)
      break;
    std::unique_ptr<Formal> Formal = parseFormal();
    if (Formal) {
      Members.emplace_back(std::move(Formal));
      continue;
    }
    if (removeUnexpected())
      continue;
    break;
  }
  if (ExpectResult ER = expect(tok_r_curly); ER.ok())
    consume();
  else
    ER.diag().note(Note::NK_ToMachThis, TokLCurly.range())
        << std::string(tok::spelling(tok_l_curly));
  return std::make_unique<Formals>(LexerCursorRange{LCur, LastToken->rCur()},
                                   std::move(Members));
}

std::unique_ptr<LambdaArg> Parser::parseLambdaArg() {
  LexerCursor LCur = lCur();
  if (Token TokID = peek(); TokID.kind() == tok_id) {
    consume(); // ID
    assert(LastToken && "LastToken should be set after consume()");
    auto ID =
        std::make_unique<Identifier>(TokID.range(), std::string(TokID.view()));
    if (peek().kind() != tok_at)
      return std::make_unique<LambdaArg>(
          LexerCursorRange{LCur, LastToken->rCur()}, std::move(ID), nullptr);

    consume(); // @
    std::unique_ptr<Formals> Formals = parseFormals();
    if (!Formals) {
      // extra "@", consider remove it.
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_LambdaArgExtraAt, TokID.range());
      D.fix("remove extra @").edit(TextEdit::mkRemoval(TokID.range()));
      D.fix("insert dummy formals")
          .edit(TextEdit::mkInsertion(TokID.rCur(), R"({})"));
    }
    return std::make_unique<LambdaArg>(
        LexerCursorRange{LCur, LastToken->rCur()}, std::move(ID),
        std::move(Formals));
  }

  std::unique_ptr<Formals> Formals = parseFormals();
  if (!Formals)
    return nullptr;
  assert(LastToken && "LastToken should be set after valid formals");
  Token TokAt = peek();
  if (TokAt.kind() != tok_at)
    return std::make_unique<LambdaArg>(
        LexerCursorRange{LCur, LastToken->rCur()}, nullptr, std::move(Formals));
  consume(); // @
  ExpectResult ER = expect(tok_id);
  if (!ER.ok()) {
    ER.diag().note(Note::NK_ToMachThis, TokAt.range())
        << std::string(tok::spelling(tok_at));
    return std::make_unique<LambdaArg>(
        LexerCursorRange{LCur, LastToken->rCur()}, nullptr, std::move(Formals));
  }
  consume(); // ID
  auto ID = std::make_unique<Identifier>(ER.tok().range(),
                                         std::string(ER.tok().view()));
  return std::make_unique<LambdaArg>(LexerCursorRange{LCur, LastToken->rCur()},
                                     std::move(ID), std::move(Formals));
}

std::unique_ptr<ExprLambda> Parser::parseExprLambda() {
  // expr_lambda : lambda_arg ':' expr
  LexerCursor LCur = lCur();
  std::unique_ptr<LambdaArg> Arg = parseLambdaArg();
  assert(LastToken && "LastToken should be set after parseLambdaArg");
  if (!Arg)
    return nullptr;
  if (ExpectResult ER = expect(tok_colon); ER.ok())
    consume();

  std::unique_ptr<Expr> Body = parseExpr();
  if (!Body)
    diagNullExpr(Diags, LastToken->rCur(), "lambda body");
  return std::make_unique<ExprLambda>(LexerCursorRange{LCur, LastToken->rCur()},
                                      std::move(Arg), std::move(Body));
}

} // namespace nixf
