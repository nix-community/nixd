/// \file
/// \brief Basic supporting functions for parsing.

#include "Parser.h"

#include "nixf/Parse/Parser.h"

using namespace nixf;

Diagnostic &detail::diagNullExpr(std::vector<Diagnostic> &Diags,
                                 LexerCursor Loc, std::string As) {
  Diagnostic &D =
      Diags.emplace_back(Diagnostic::DK_Expected, LexerCursorRange(Loc));
  D << std::move(As) + " expression";
  D.fix("insert dummy expression").edit(TextEdit::mkInsertion(Loc, " expr"));
  return D;
}

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

Parser::SyncRAII Parser::withSync(TokenKind Kind) { return {*this, Kind}; }

std::shared_ptr<Node> nixf::parse(std::string_view Src,
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
