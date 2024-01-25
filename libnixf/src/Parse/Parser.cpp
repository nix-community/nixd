/// \file
/// \brief Parser implementation.
#pragma once

#include "Lexer.h"
#include "Token.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Parser.h"

#include <cassert>
#include <charconv>
#include <deque>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <string_view>

namespace {

using namespace nixf;
using namespace nixf::tok;

Diagnostic &diagNullExpr(std::vector<Diagnostic> &Diags, LexerCursor Loc,
                         std::string As) {
  Diagnostic &D =
      Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Loc));
  D << std::move(As) + " expression";
  D.fix("insert dummy expression").edit(TextEdit::mkInsertion(Loc, " expr"));
  return D;
}

class Parser {
public:
  enum ParserState {
    PS_Expr,
    PS_String,
    PS_IndString,
    PS_Path,
  };

private:
  std::string_view Src;
  Lexer Lex;
  std::vector<Diagnostic> &Diags;

  std::deque<Token> LookAheadBuf;
  std::optional<Token> LastToken;
  std::stack<ParserState> State;

  /// \brief Sync tokens for error recovery.
  ///
  /// These tokens will be considered as the end of "unknown" node.
  /// We create "unknown" node for recover from "extra" token error.
  /// (Also, this node is invisible in the AST)
  ///
  /// e.g. { foo....bar = ; }
  ///            ^~~  remove these tokens
  ///
  /// Sync tokens will not be consumed as "unknown".
  std::multiset<TokenKind> SyncTokens;

  class StateRAII {
    Parser &P;

  public:
    StateRAII(Parser &P) : P(P) {}
    ~StateRAII() { P.popState(); }
  };

  // Note: use `auto` for this type.
  StateRAII withState(ParserState NewState) {
    pushState(NewState);
    return {*this};
  }

  /// \brief Reset the lexer cursor to the beginning of the first token.
  ///
  /// This is used for error recovery & context switching.
  void resetLookAheadBuf() {
    if (!LookAheadBuf.empty()) {
      Token Tok = LookAheadBuf.front();

      // Reset the lexer cursor at the beginning of the token.
      Lex.setCur(Tok.begin());
      LookAheadBuf.clear();
    }
  }

  void pushState(ParserState NewState) {
    resetLookAheadBuf();
    State.push(NewState);
  }

  void popState() {
    resetLookAheadBuf();
    State.pop();
  }

  Token peek(std::size_t N = 0) {
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

  /// \brief Consume tokens until the next sync token.
  /// \returns The consumed range. If no token is consumed, return nullopt.
  std::optional<LexerCursorRange> consumeAsUnknown() {
    LexerCursor Begin = peek().begin();
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
    return LexerCursorRange{Begin, LastToken->end()};
  }

  class SyncRAII {
    Parser &P;
    TokenKind Kind;

  public:
    SyncRAII(Parser &P, TokenKind Kind) : P(P), Kind(Kind) {
      P.SyncTokens.emplace(Kind);
    }
    ~SyncRAII() { P.SyncTokens.erase(Kind); }
  };

  SyncRAII withSync(TokenKind Kind) { return {*this, Kind}; }

  class ExpectResult {
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

  ExpectResult expect(TokenKind Kind) {
    auto Sync = withSync(Kind);
    if (Token Tok = peek(); Tok.kind() == Kind) {
      return Tok;
    }
    // UNKNOWN ?
    // ~~~~~~~ consider remove unexpected text
    if (std::optional<LexerCursorRange> UnknownRange = consumeAsUnknown()) {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_UnexpectedText, *UnknownRange);
      D.fix("remove unexpected text").edit(TextEdit::mkRemoval(*UnknownRange));

      if (Token Tok = peek(); Tok.kind() == Kind) {
        return Tok;
      }
      // If the next token is not the expected one, then insert it.
      // (we have two errors now).
    }
    // expected Kind
    LexerCursor Insert = LastToken ? LastToken->end() : peek().begin();
    Diagnostic &D =
        Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Insert));
    D << std::string(tok::spelling(Kind));
    D.fix("insert " + std::string(tok::spelling(Kind)))
        .edit(TextEdit::mkInsertion(Insert, std::string(tok::spelling(Kind))));
    return {&D};
  }

  void consume() {
    if (LookAheadBuf.empty())
      peek(0);
    popBuf();
  }

  Token popBuf() {
    LastToken = LookAheadBuf.front();
    LookAheadBuf.pop_front();
    return *LastToken;
  }

public:
  Parser(std::string_view Src, std::vector<Diagnostic> &Diags)
      : Src(Src), Lex(Src, Diags), Diags(Diags) {
    pushState(PS_Expr);
  }

  /// \brief Parse interpolations.
  ///
  /// interpolation : "${" expr "}"
  std::shared_ptr<Expr> parseInterpolation() {
    Token TokDollarCurly = peek();
    assert(TokDollarCurly.kind() == tok_dollar_curly);
    consume(); // ${
    auto Sync = withSync(tok_r_curly);
    assert(LastToken);
    /* with(PS_Expr) */ {
      auto ExprState = withState(PS_Expr);
      auto Expr = parseExpr();
      if (!Expr)
        diagNullExpr(Diags, LastToken->end(), "interpolation");
      if (ExpectResult ER = expect(tok_r_curly); ER.ok()) {
        consume(); // }
      } else {
        ER.diag().note(Note::NK_ToMachThis, TokDollarCurly.range())
            << std::string(tok::spelling(tok_dollar_curly));
      }
      return Expr;
    } // with(PS_Expr)
    return nullptr;
  }

  /// \brief Parse paths.
  ///
  ///  path : path_fragment (path_fragment)*  path_end
  /// Context     PS_Expr       PS_Path        PS_Path
  ///
  /// The first token, path_fragment is lexed in PS_Expr context, then switch in
  /// "PS_Path" context. The ending token "path_end" shall be poped with context
  /// switching.
  std::shared_ptr<Expr> parseExprPath() {
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
        End = Current.end();
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
    auto Parts = std::make_shared<InterpolatedParts>(
        LexerCursorRange{Begin.begin(), End}, std::move(Fragments));
    return std::make_shared<ExprPath>(LexerCursorRange{Begin.begin(), End},
                                      std::move(Parts));
  }

  /// string_part : interpolation
  ///             | STRING_PART
  ///             | STRING_ESCAPE
  std::shared_ptr<InterpolatedParts> parseStringParts() {
    std::vector<InterpolablePart> Parts;
    LexerCursor PartsBegin = peek().begin();
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
        return std::make_shared<InterpolatedParts>(
            LexerCursorRange{PartsBegin, LastToken->end()},
            std::move(Parts)); // TODO!
      }
    }
  }

  /// string : " string_part* "
  ///        | '' string_part* ''
  std::shared_ptr<ExprString> parseString(bool IsIndented) {
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
      std::shared_ptr<InterpolatedParts> Parts = parseStringParts();
      if (ExpectResult ER = expect(QuoteKind); ER.ok()) {
        consume();
        return std::make_shared<ExprString>(
            LexerCursorRange{Quote.begin(), ER.tok().end()}, std::move(Parts));
      } else { // NOLINT(readability-else-after-return)
        ER.diag().note(Note::NK_ToMachThis, Quote.range()) << QuoteSpel;
        return std::make_shared<ExprString>(
            LexerCursorRange{Quote.begin(), Parts->end()}, std::move(Parts));
      }

    } // with(PS_String / PS_IndString)
  }

  /// '(' expr ')'
  std::shared_ptr<ExprParen> parseExprParen() {
    Token L = peek();
    auto LParen = std::make_shared<Misc>(L.range());
    assert(L.kind() == tok_l_paren);
    consume(); // (
    auto Sync = withSync(tok_r_paren);
    assert(LastToken && "LastToken should be set after consume()");
    auto Expr = parseExpr();
    if (!Expr)
      diagNullExpr(Diags, LastToken->end(), "parenthesized");
    if (ExpectResult ER = expect(tok_r_paren); ER.ok()) {
      consume(); // )
      auto RParen = std::make_shared<Misc>(ER.tok().range());
      return std::make_shared<ExprParen>(
          LexerCursorRange{L.begin(), ER.tok().end()}, std::move(Expr),
          std::move(LParen), std::move(RParen));
    } else { // NOLINT(readability-else-after-return)
      ER.diag().note(Note::NK_ToMachThis, L.range())
          << std::string(tok::spelling(tok_l_paren));
      return std::make_shared<ExprParen>(
          LexerCursorRange{L.begin(), LastToken->end()}, std::move(Expr),
          std::move(LParen),
          /*RParen=*/nullptr);
    }
  }

  /// attrname : ID
  ///          | string
  ///          | interpolation
  std::shared_ptr<AttrName> parseAttrName() {
    switch (Token Tok = peek(); Tok.kind()) {
    case tok_kw_or:
      Diags.emplace_back(Diagnostic::DK_OrIdentifier, Tok.range());
      [[fallthrough]];
    case tok_id: {
      consume();
      auto ID =
          std::make_shared<Identifier>(Tok.range(), std::string(Tok.view()));
      return std::make_shared<AttrName>(std::move(ID), Tok.range());
    }
    case tok_dquote: {
      std::shared_ptr<ExprString> String = parseString(/*IsIndented=*/false);
      return std::make_shared<AttrName>(std::move(String), Tok.range());
    }
    case tok_dollar_curly: {
      std::shared_ptr<Expr> Expr = parseInterpolation();
      return std::make_shared<AttrName>(std::move(Expr), Tok.range());
    }
    default:
      return nullptr;
    }
  }

  /// attrpath : attrname ('.' attrname)*
  std::shared_ptr<AttrPath> parseAttrPath() {
    auto First = parseAttrName();
    if (!First)
      return nullptr;
    LexerCursor Begin = First->begin();
    assert(LastToken && "LastToken should be set after valid attrname");
    std::vector<std::shared_ptr<AttrName>> AttrNames;
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
              .edit(TextEdit::mkInsertion(Tok.range().end(), R"("dummy")"));
        }
        AttrNames.emplace_back(std::move(Next));
        continue;
      }
      break;
    }
    return std::make_shared<AttrPath>(LexerCursorRange{Begin, LastToken->end()},
                                      std::move(AttrNames));
  }

  /// binding : attrpath '=' expr ';'
  std::shared_ptr<Binding> parseBinding() {
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
      diagNullExpr(Diags, LastToken->end(), "binding");
    if (Token Tok = peek(); Tok.kind() == tok_semi_colon) {
      consume();
    } else {
      // TODO: reset the cursor for error recovery.
      // (https://github.com/nix-community/nixd/blob/2b0ca8cef0d13823132a52b6cd6f6d7372482664/libnixf/lib/Parse/Parser.cpp#L337)
      // expected ";" for binding
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_Expected,
                                         LexerCursorRange(LastToken->end()));
      D << std::string(tok::spelling(tok_semi_colon));
      D.fix("insert ;").edit(TextEdit::mkInsertion(LastToken->end(), ";"));
    }
    return std::make_shared<Binding>(
        LexerCursorRange{Path->begin(), LastToken->end()}, std::move(Path),
        std::move(Expr));
  }

  /// inherit :  'inherit' '(' expr ')' inherited_attrs ';'
  ///         |  'inherit' inherited_attrs ';'
  /// inherited_attrs: attrname*
  std::shared_ptr<Inherit> parseInherit() {
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
    std::vector<std::shared_ptr<AttrName>> AttrNames;
    std::shared_ptr<Expr> Expr = nullptr;
    if (Token Tok = peek(); Tok.kind() == tok_l_paren) {
      consume();
      Expr = parseExpr();
      if (!Expr)
        diagNullExpr(Diags, LastToken->end(), "inherit");
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
          << std::string(tok::spelling(tok_semi_colon));
    return std::make_shared<Inherit>(
        LexerCursorRange{TokInherit.begin(), LastToken->end()},
        std::move(AttrNames), std::move(Expr));
  }

  /// binds : ( binding | inherit )*
  std::shared_ptr<Binds> parseBinds() {
    // TODO: curently we don't support inherit
    LexerCursor Begin = peek().begin();
    std::vector<std::shared_ptr<Node>> Bindings;
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
    return std::make_shared<Binds>(LexerCursorRange{Begin, LastToken->end()},
                                   std::move(Bindings));
  }

  /// attrset_expr : REC? '{' binds '}'
  ///
  /// Note: peek `tok_kw_rec` or `tok_l_curly` before calling this function.
  std::shared_ptr<ExprAttrs> parseExprAttrs() {
    std::shared_ptr<Misc> Rec;

    auto Sync = withSync(tok_r_curly);

    // "to match this ..."
    // if "{" is missing, then use "rec", otherwise use "{"
    Token Matcher = peek();
    LexerCursor Begin = peek().begin(); // rec or {
    if (Token Tok = peek(); Tok.kind() == tok_kw_rec) {
      consume();
      Rec = std::make_shared<Misc>(Tok.range());
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
    return std::make_shared<ExprAttrs>(
        LexerCursorRange{Begin, LastToken->end()}, std::move(Binds),
        std::move(Rec));
  }

  /// expr_simple :  INT
  ///             | ID
  ///             | FLOAT
  ///             | string
  ///             | indented_string
  ///             | path
  ///             | hpath
  ///             | uri
  ///             | '(' expr ')'
  ///             | legacy_let
  ///             | attrset_expr
  ///             | list
  std::shared_ptr<Expr> parseExprSimple() {
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
      NixInt N;
      auto [_, Err] = std::from_chars(Tok.view().begin(), Tok.view().end(), N);
      assert(Err == std::errc());
      return std::make_shared<ExprInt>(Tok.range(), N);
    }
    case tok_float: {
      consume();
      // libc++ doesn't support std::from_chars for floating point numbers.
      NixFloat N = std::strtof(std::string(Tok.view()).c_str(), nullptr);
      return std::make_shared<ExprFloat>(Tok.range(), N);
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
    default:
      return nullptr;
    }
  }

  std::shared_ptr<Expr> parseExpr() {
    return parseExprSimple(); // TODO!
  }
  std::shared_ptr<Expr> parse() { return parseExpr(); }
};

} // namespace

namespace nixf {

std::shared_ptr<Node> parse(std::string_view Src,
                            std::vector<Diagnostic> &Diags) {
  Parser P(Src, Diags);
  return P.parse();
}

} // namespace nixf
