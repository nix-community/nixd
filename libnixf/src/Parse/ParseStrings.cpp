#include "ParserImpl.h"

#include "nixf/Basic/Nodes/Simple.h"

namespace nixf {

using namespace detail;

std::unique_ptr<Interpolation> Parser::parseInterpolation() {
  Token TokDollarCurly = peek();
  assert(TokDollarCurly.kind() == tok_dollar_curly);
  consume(); // ${
  auto Sync = withSync(tok_r_curly);
  assert(LastToken);
  /* with(PS_Expr) */ {
    auto ExprState = withState(PS_Expr);
    auto Expr = parseExpr();
    if (!Expr)
      diagNullExpr(Diags, LastToken->rCur(), "interpolation");
    if (ExpectResult ER = expect(tok_r_curly); ER.ok()) {
      consume(); // }
    } else {
      ER.diag().note(Note::NK_ToMachThis, TokDollarCurly.range())
          << std::string(tok::spelling(tok_dollar_curly));
    }
    return std::make_unique<Interpolation>(
        LexerCursorRange{TokDollarCurly.lCur(), LastToken->rCur()},
        std::move(Expr));
  } // with(PS_Expr)
}

std::unique_ptr<Expr> Parser::parseExprPath() {
  Token Begin = peek();
  std::vector<InterpolablePart> Fragments;
  assert(Begin.kind() == tok_path_fragment);
  LexerCursor End;
  /* with(PS_Path) */ {
    auto PathState = withState(PS_Path);
    do {
      Token Current = peek();
      Fragments.emplace_back(std::string(Current.view()));
      consume();
      End = Current.rCur();
      Token Next = peek();
      if (Next.kind() == tok_path_end)
        break;
      if (Next.kind() == tok_dollar_curly) {
        if (auto Expr = parseInterpolation())
          Fragments.emplace_back(std::move(Expr));
        continue;
      }
      assert(false && "should be path_end or ${");
    } while (true);
  }
  auto Parts = std::make_unique<InterpolatedParts>(
      LexerCursorRange{Begin.lCur(), End}, std::move(Fragments));
  return std::make_unique<ExprPath>(LexerCursorRange{Begin.lCur(), End},
                                    std::move(Parts));
}

std::unique_ptr<InterpolatedParts> Parser::parseStringParts() {
  std::vector<InterpolablePart> Parts;
  LexerCursor PartsBegin = peek().lCur();
  while (true) {
    switch (Token Tok = peek(0); Tok.kind()) {
    case tok_dollar_curly: {
      if (auto Expr = parseInterpolation())
        Parts.emplace_back(std::move(Expr));
      continue;
    }
    case tok_string_part: {
      // If this is a part of string, just push it.
      Parts.emplace_back(std::string(Tok.view()));
      consume();
      continue;
    }
    case tok_string_escape:
      // If this is a part of string, just push it.
      consume();
      // TODO: escape and emplace_back
      continue;
    default:
      assert(LastToken && "LastToken should be set in `parseString`");
      return std::make_unique<InterpolatedParts>(
          LexerCursorRange{PartsBegin, LastToken->rCur()},
          std::move(Parts)); // TODO!
    }
  }
}

std::unique_ptr<ExprString> Parser::parseString(bool IsIndented) {
  Token Quote = peek();
  TokenKind QuoteKind = IsIndented ? tok_quote2 : tok_dquote;
  std::string QuoteSpel(tok::spelling(QuoteKind));
  assert(Quote.kind() == QuoteKind && "should be a quote");
  // Consume the quote and so make the look-ahead buf empty.
  consume();
  auto Sync = withSync(QuoteKind);
  assert(LastToken && "LastToken should be set after consume()");
  /* with(PS_String / PS_IndString) */ {
    auto StringState = withState(IsIndented ? PS_IndString : PS_String);
    std::unique_ptr<InterpolatedParts> Parts = parseStringParts();
    if (ExpectResult ER = expect(QuoteKind); ER.ok()) {
      consume();
      return std::make_unique<ExprString>(
          LexerCursorRange{Quote.lCur(), ER.tok().rCur()}, std::move(Parts));
    } else { // NOLINT(readability-else-after-return)
      ER.diag().note(Note::NK_ToMachThis, Quote.range()) << QuoteSpel;
      return std::make_unique<ExprString>(
          LexerCursorRange{Quote.lCur(), Parts->rCur()}, std::move(Parts));
    }

  } // with(PS_String / PS_IndString)
}

} // namespace nixf
