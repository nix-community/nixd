#include "Parser.h"

namespace nixf {

using namespace detail;

std::unique_ptr<AttrName> Parser::parseAttrName() {
  switch (Token Tok = peek(); Tok.kind()) {
  case tok_kw_or:
    Diags.emplace_back(Diagnostic::DK_OrIdentifier, Tok.range());
    [[fallthrough]];
  case tok_id: {
    consume();
    auto ID =
        std::make_unique<Identifier>(Tok.range(), std::string(Tok.view()));
    return std::make_unique<AttrName>(std::move(ID), Tok.range());
  }
  case tok_dquote: {
    std::unique_ptr<ExprString> String = parseString(/*IsIndented=*/false);
    return std::make_unique<AttrName>(std::move(String));
  }
  case tok_dollar_curly: {
    std::unique_ptr<Interpolation> Expr = parseInterpolation();
    return std::make_unique<AttrName>(std::move(Expr));
  }
  default:
    return nullptr;
  }
}

std::unique_ptr<AttrPath> Parser::parseAttrPath() {
  auto First = parseAttrName();
  if (!First)
    return nullptr;
  LexerCursor Begin = First->lCur();
  assert(LastToken && "LastToken should be set after valid attrname");
  std::vector<std::unique_ptr<AttrName>> AttrNames;
  AttrNames.emplace_back(std::move(First));
  while (true) {
    if (Token Tok = peek(); Tok.kind() == tok_dot) {
      consume();
      auto Next = parseAttrName();
      if (!Next) {
        // extra ".", consider remove it.
        Diagnostic &D =
            Diags.emplace_back(Diagnostic::DK_AttrPathExtraDot, Tok.range());
        D.fix("remove extra .").edit(TextEdit::mkRemoval(Tok.range()));
        D.fix("insert dummy attrname")
            .edit(TextEdit::mkInsertion(Tok.rCur(), R"("dummy")"));
      }
      AttrNames.emplace_back(std::move(Next));
      continue;
    }
    break;
  }
  return std::make_unique<AttrPath>(LexerCursorRange{Begin, LastToken->rCur()},
                                    std::move(AttrNames));
}

std::unique_ptr<Binding> Parser::parseBinding() {
  auto Path = parseAttrPath();
  if (!Path)
    return nullptr;
  assert(LastToken && "LastToken should be set after valid attrpath");
  auto SyncEq = withSync(tok_eq);
  auto SyncSemi = withSync(tok_semi_colon);
  if (ExpectResult ER = expect(tok_eq); !ER.ok())
    return std::make_unique<Binding>(
        LexerCursorRange{Path->lCur(), LastToken->rCur()}, std::move(Path),
        nullptr);
  consume();
  auto Expr = parseExpr();
  if (!Expr)
    diagNullExpr(Diags, LastToken->rCur(), "binding");
  if (Token Tok = peek(); Tok.kind() == tok_semi_colon) {
    consume();
  } else {
    // TODO: reset the cursor for error recovery.
    // (https://github.com/nix-community/nixd/blob/2b0ca8cef0d13823132a52b6cd6f6d7372482664/libnixf/lib/Parse/Parser.cpp#L337)
    // expected ";" for binding
    Diagnostic &D = Diags.emplace_back(Diagnostic::DK_Expected,
                                       LexerCursorRange(LastToken->rCur()));
    D << std::string(tok::spelling(tok_semi_colon));
    D.fix("insert ;").edit(TextEdit::mkInsertion(LastToken->rCur(), ";"));
  }
  return std::make_unique<Binding>(
      LexerCursorRange{Path->lCur(), LastToken->rCur()}, std::move(Path),
      std::move(Expr));
}

std::unique_ptr<Inherit> Parser::parseInherit() {
  Token TokInherit = peek();
  if (TokInherit.kind() != tok_kw_inherit)
    return nullptr;
  consume();
  auto SyncSemiColon = withSync(tok_semi_colon);

  // These tokens might be consumed as "inherited_attrs"
  auto SyncID = withSync(tok_id);
  auto SyncQuote = withSync(tok_dquote);
  auto SyncDollarCurly = withSync(tok_dollar_curly);

  assert(LastToken && "LastToken should be set after consume()");
  std::vector<std::unique_ptr<AttrName>> AttrNames;
  std::unique_ptr<Expr> Expr = nullptr;
  if (Token Tok = peek(); Tok.kind() == tok_l_paren) {
    consume();
    Expr = parseExpr();
    if (!Expr)
      diagNullExpr(Diags, LastToken->rCur(), "inherit");
    if (ExpectResult ER = expect(tok_r_paren); ER.ok())
      consume();
    else
      ER.diag().note(Note::NK_ToMachThis, Tok.range())
          << std::string(tok::spelling(tok_l_paren));
  }
  while (true) {
    if (auto AttrName = parseAttrName()) {
      AttrNames.emplace_back(std::move(AttrName));
      continue;
    }
    break;
  }
  if (ExpectResult ER = expect(tok_semi_colon); ER.ok())
    consume();
  else
    ER.diag().note(Note::NK_ToMachThis, TokInherit.range())
        << std::string(tok::spelling(tok_kw_inherit));
  return std::make_unique<Inherit>(
      LexerCursorRange{TokInherit.lCur(), LastToken->rCur()},
      std::move(AttrNames), std::move(Expr));
}

std::unique_ptr<Binds> Parser::parseBinds() {
  // TODO: curently we don't support inherit
  LexerCursor Begin = peek().lCur();
  std::vector<std::unique_ptr<Node>> Bindings;
  // attrpath
  auto SyncID = withSync(tok_id);
  auto SyncQuote = withSync(tok_dquote);
  auto SyncDollarCurly = withSync(tok_dollar_curly);

  // inherit
  auto SyncInherit = withSync(tok_kw_inherit);
  while (true) {
    if (auto Binding = parseBinding()) {
      Bindings.emplace_back(std::move(Binding));
      continue;
    }
    if (auto Inherit = parseInherit()) {
      Bindings.emplace_back(std::move(Inherit));
      continue;
    }
    // If it is neither a binding, nor an inherit. Let's consume an "Unknown"
    // For error recovery
    if (removeUnexpected())
      continue;
    break;
  }
  if (Bindings.empty())
    return nullptr;
  assert(LastToken && "LastToken should be set after valid binding");
  return std::make_unique<Binds>(LexerCursorRange{Begin, LastToken->rCur()},
                                 std::move(Bindings));
}

std::unique_ptr<ExprAttrs> Parser::parseExprAttrs() {
  std::unique_ptr<Misc> Rec;

  auto Sync = withSync(tok_r_curly);

  // "to match this ..."
  // if "{" is missing, then use "rec", otherwise use "{"
  Token Matcher = peek();
  LexerCursor Begin = peek().lCur(); // rec or {
  if (Token Tok = peek(); Tok.kind() == tok_kw_rec) {
    consume();
    Rec = std::make_unique<Misc>(Tok.range());
  }
  if (ExpectResult ER = expect(tok_l_curly); ER.ok()) {
    consume();
    Matcher = ER.tok();
  }
  assert(LastToken && "LastToken should be set after valid { or rec");
  auto Binds = parseBinds();
  if (ExpectResult ER = expect(tok_r_curly); ER.ok())
    consume();
  else
    ER.diag().note(Note::NK_ToMachThis, Matcher.range())
        << std::string(tok::spelling(Matcher.kind()));
  return std::make_unique<ExprAttrs>(LexerCursorRange{Begin, LastToken->rCur()},
                                     std::move(Binds), std::move(Rec));
}

} // namespace nixf
