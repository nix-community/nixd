/// \file
/// \brief Parser implementation.
#include "Parser.h"
#include "nixf/Basic/Nodes.h"

#include <charconv>

namespace {

using namespace nixf;

Diagnostic &diagNullExpr(std::vector<Diagnostic> &Diags, LexerCursor Loc,
                         std::string As) {
  Diagnostic &D =
      Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Loc));
  D << std::move(As) + " expression";
  D.fix("insert dummy expression").edit(TextEdit::mkInsertion(Loc, " expr"));
  return D;
}

} // namespace

namespace nixf {

class Parser::StateRAII {
  Parser &P;

public:
  StateRAII(Parser &P) : P(P) {}
  ~StateRAII() { P.popState(); }
};

void Parser::pushState(ParserState NewState) {
  resetLookAheadBuf();
  State.push(NewState);
}

void Parser::popState() {
  resetLookAheadBuf();
  State.pop();
}

Parser::StateRAII Parser::withState(ParserState NewState) {
  pushState(NewState);
  return {*this};
}

class Parser::SyncRAII {
  Parser &P;
  TokenKind Kind;

public:
  SyncRAII(Parser &P, TokenKind Kind) : P(P), Kind(Kind) {
    P.SyncTokens.emplace(Kind);
  }
  ~SyncRAII() { P.SyncTokens.erase(P.SyncTokens.find(Kind)); }
};

Parser::SyncRAII Parser::withSync(TokenKind Kind) { return {*this, Kind}; }

class Parser::ExpectResult {
  bool Success;
  std::optional<Token> Tok;
  Diagnostic *DiagMissing;

public:
  ExpectResult(Token Tok) : Success(true), Tok(Tok), DiagMissing(nullptr) {}
  ExpectResult(Diagnostic *DiagMissing)
      : Success(false), DiagMissing(DiagMissing) {}

  [[nodiscard]] bool ok() const { return Success; }
  [[nodiscard]] Token tok() const {
    assert(Tok);
    return *Tok;
  }
  [[nodiscard]] Diagnostic &diag() const {
    assert(DiagMissing);
    return *DiagMissing;
  }
};

std::unique_ptr<Node> parse(std::string_view Src,
                            std::vector<Diagnostic> &Diags) {
  Parser P(Src, Diags);
  return P.parse();
}

void Parser::resetLookAheadBuf() {
  if (!LookAheadBuf.empty()) {
    Token Tok = LookAheadBuf.front();

    // Reset the lexer cursor at the beginning of the token.
    Lex.setCur(Tok.lCur());
    LookAheadBuf.clear();
  }
}

Token Parser::peek(std::size_t N) {
  while (N >= LookAheadBuf.size()) {
    switch (State.top()) {
    case PS_Expr:
      LookAheadBuf.emplace_back(Lex.lex());
      break;
    case PS_String:
      LookAheadBuf.emplace_back(Lex.lexString());
      break;
    case PS_IndString:
      LookAheadBuf.emplace_back(Lex.lexIndString());
      break;
    case PS_Path:
      LookAheadBuf.emplace_back(Lex.lexPath());
      break;
    }
  }
  return LookAheadBuf[N];
}

std::optional<LexerCursorRange> Parser::consumeAsUnknown() {
  LexerCursor Begin = peek().lCur();
  bool Consumed = false;
  for (Token Tok = peek(); Tok.kind() != tok_eof; Tok = peek()) {
    if (SyncTokens.contains(Tok.kind()))
      break;
    Consumed = true;
    consume();
  }
  if (!Consumed)
    return std::nullopt;
  assert(LastToken && "LastToken should be set after consume()");
  return LexerCursorRange{Begin, LastToken->rCur()};
}

Parser::ExpectResult Parser::expect(TokenKind Kind) {
  auto Sync = withSync(Kind);
  if (Token Tok = peek(); Tok.kind() == Kind) {
    return Tok;
  }
  // UNKNOWN ?
  // ~~~~~~~ consider remove unexpected text
  if (removeUnexpected()) {
    if (Token Tok = peek(); Tok.kind() == Kind) {
      return Tok;
    }
    // If the next token is not the expected one, then insert it.
    // (we have two errors now).
  }
  // expected Kind
  LexerCursor Insert = LastToken ? LastToken->rCur() : peek().lCur();
  Diagnostic &D =
      Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Insert));
  D << std::string(tok::spelling(Kind));
  D.fix("insert " + std::string(tok::spelling(Kind)))
      .edit(TextEdit::mkInsertion(Insert, std::string(tok::spelling(Kind))));
  return {&D};
}

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
  if (ExpectResult ER = expect(tok_eq); ER.ok())
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
  while (true) {
    if (auto Binding = parseBinding()) {
      Bindings.emplace_back(std::move(Binding));
      continue;
    }
    if (auto Inherit = parseInherit()) {
      Bindings.emplace_back(std::move(Inherit));
      continue;
    }
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

} // namespace nixf
