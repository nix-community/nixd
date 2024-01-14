/// \file
/// \brief Parser implementation.
#pragma once

#include "Lexer.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Nodes.h"
#include "nixf/Parse/Parser.h"

#include <cassert>
#include <charconv>
#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <string_view>

namespace {

using namespace nixf;
using namespace nixf::tok;

Diagnostic &diagNullExpr(DiagnosticEngine &Diag, Point Loc, std::string As) {
  Diagnostic &D = Diag.diag(Diagnostic::DK_Expected, RangeTy(Loc));
  D << ("an expression as " + std::move(As));
  D.fix(Fix::mkInsertion(Loc, " expr"));
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
  DiagnosticEngine &Diag;

  std::deque<Token> LookAheadBuf;
  std::optional<Token> LastToken;
  std::stack<ParserState> State;

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
  Parser(std::string_view Src, DiagnosticEngine &Diag)
      : Src(Src), Lex(Src, Diag), Diag(Diag) {
    pushState(PS_Expr);
  }

  /// \brief Parse interpolations.
  ///
  /// interpolation : "${" expr "}"
  std::shared_ptr<Expr> parseInterpolation() {
    assert(peek().kind() == tok_dollar_curly);
    consume();
    assert(LastToken);
    /* with(PS_Expr) */ {
      auto ExprState = withState(PS_Expr);
      return parseExpr();
    } // with(PS_Expr)
    return nullptr;
  }

  /// \brief Parse interpolable things.
  ///
  /// They are strings, ind-strings, paths, in nix language.
  /// \note This needs context-switching so look-ahead buf should be cleared.
  std::shared_ptr<InterpolatedParts> parseStringParts() {
    std::vector<InterpolablePart> Parts;
    Point PartsBegin = peek().begin();
    while (true) {
      switch (Token Tok = peek(0); Tok.kind()) {
      case tok_dollar_curly: {
        if (auto Expr = parseInterpolation()) {
          Parts.emplace_back(std::move(Expr));
        } else {
          assert(LastToken); // at least "${"
          diagNullExpr(Diag, LastToken->end(), "string interpolation");
        }
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
            RangeTy{
                PartsBegin,
                LastToken->end(),
            },
            std::move(Parts)); // TODO!
      }
    }
  }

  std::shared_ptr<Expr> parseString(bool IsIndented) {
    Token Quote = peek();
    TokenKind QuoteKind = IsIndented ? tok_quote2 : tok_dquote;
    std::string QuoteSpel(tok::spelling(QuoteKind));
    assert(Quote.kind() == QuoteKind && "should be a quote");
    // Consume the quote and so make the look-ahead buf empty.
    consume();
    assert(LastToken && "LastToken should be set after consume()");
    /* with(PS_String / PS_IndString) */ {
      auto StringState = withState(IsIndented ? PS_IndString : PS_String);
      std::shared_ptr<InterpolatedParts> Parts = parseStringParts();
      if (Token EndTok = peek(); EndTok.kind() == QuoteKind) {
        consume();
        return std::make_shared<ExprString>(
            RangeTy{
                Quote.begin(),
                EndTok.end(),
            },
            std::move(Parts));
      }
      Diagnostic &D =
          Diag.diag(Diagnostic::DK_Expected, RangeTy(LastToken->end()));
      D << QuoteSpel;
      D.note(Note::NK_ToMachThis, Quote.range()) << QuoteSpel;
      D.fix(Fix::mkInsertion(LastToken->end(), QuoteSpel));
      return std::make_shared<ExprString>(
          RangeTy{
              Quote.begin(),
              Parts->end(),
          },
          std::move(Parts));

    } // with(PS_String / PS_IndString)
  }

  std::shared_ptr<Expr> parseExprSimple() {
    Token Tok = peek();
    switch (Tok.kind()) {
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

std::shared_ptr<Node> parse(std::string_view Src, DiagnosticEngine &Diag) {
  Parser P(Src, Diag);
  return P.parse();
}

} // namespace nixf
