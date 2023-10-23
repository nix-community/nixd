#include "nixf/Lex/Lexer.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Syntax/Token.h"
#include "nixf/Syntax/Trivia.h"

#include <cassert>
#include <cctype>

namespace nixf {

using namespace tok;

using DK = Diagnostic::DiagnosticKind;
using NK = Note::NoteKind;

bool Lexer::consumePrefix(std::string_view Prefix) {
  // Check [Cur, Cur + Prefix.length)
  if (Cur + Prefix.length() > Src.end())
    return false;
  if (remain().starts_with(Prefix)) {
    Cur += Prefix.length();
    return true;
  }
  return false;
}

std::optional<TriviaPiece> Lexer::tryConsumeWhitespaces() {
  if (eof() || !std::isspace(*Cur))
    return std::nullopt;

  char Ch = *Cur;
  unsigned Count = 0;
  TriviaKind Kind = spaceTriviaKind(Ch);

  // consume same characters and combine them into a TriviaPiece
  do {
    Count++;
    Cur++;
  } while (Cur != Src.end() && *Cur == Ch);

  switch (Kind) {
  case TriviaKind::Space:
    return TriviaPiece::spaces(Count);
  case TriviaKind::Tab:
    return TriviaPiece::tabs(Count);
  case TriviaKind::VerticalTab:
    return TriviaPiece::verticalTabs(Count);
  case TriviaKind::Formfeed:
    return TriviaPiece::formFeeds(Count);
  case TriviaKind::Newline:
    return TriviaPiece::newlines(Count);
  case TriviaKind::CarriageReturn:
    return TriviaPiece::carriageReturns(Count);
  default:
    __builtin_unreachable();
  }
}

bool Lexer::tryAdvanceEOL() {
  if (!eof(Cur + 1) && remain().starts_with("\r\n")) {
    // CRLF
    Cur += 2;
    return true;
  }
  if (!eof() && *Cur == '\n') {
    // LF
    Cur++;
    return true;
  }
  return false;
}

std::optional<TriviaPiece> Lexer::tryConsumeComments() {
  if (eof())
    return std::nullopt;

  std::string_view Remain = remain();
  const char *BeginPtr = Cur;
  unsigned BeginOffset = curOffset();

  if (consumePrefix("/*")) {
    // consume block comments until we meet '*/'
    while (true) {
      if (eof()) {
        // there is no '*/' to terminate comments
        OffsetRange R = {curOffset() - 1, curOffset()};
        OffsetRange B = {BeginOffset, BeginOffset + 2};

        Diags.diag(DK::DK_UnterminatedBComment, R)
            .note(NK::NK_BCommentBegin, B);

        // recover
        return TriviaPiece::blockComment(std::string(Remain));
      }
      if (!eof(Cur + 1) && consumePrefix("*/"))
        // we found the ending '*/'
        return TriviaPiece::blockComment(std::string(BeginPtr, Cur));
      Cur++;
    }
  } else if (consumePrefix("#")) {
    // single line comments, consume blocks until we meet EOF or '\n' or '\r'
    while (true) {
      if (eof() || tryAdvanceEOL()) {
        return TriviaPiece::lineComment(std::string(BeginPtr, Cur));
      }
      Cur++;
    }
  } else {
    return std::nullopt;
  }
}

Trivia Lexer::consumeTrivia() {
  if (eof())
    return {};

  Trivia Result;
  while (true) {
    if (std::optional<TriviaPiece> OTP = tryConsumeWhitespaces())
      Result.Pieces.emplace_back(std::move(*OTP));
    else if (std::optional<TriviaPiece> OTP = tryConsumeComments())
      Result.Pieces.emplace_back(std::move(*OTP));
    else
      break;
  }

  return Result;
}

bool Lexer::lexFloatExp(std::string &NumStr) {
  // accept ([Ee][+-]?[0-9]+)?, the exponential part (after `.` of a float)
  if (!eof() && (*Cur == 'E' || *Cur == 'e')) {
    unsigned EOffset = curOffset();
    char ECh = *Cur;
    NumStr += *Cur;
    Cur++;
    // [+-]?
    if (!eof() && (*Cur == '+' || *Cur == '-')) {
      NumStr += *Cur;
      Cur++;
    }
    // [0-9]+
    if (!eof() && isdigit(*Cur)) {
      do {
        NumStr += *Cur;
        Cur++;
      } while (!eof() && isdigit(*Cur));
    } else {
      // not matching [0-9]+, error
      Diags.diag(DK::DK_FloatNoExp, {EOffset, EOffset + 1}) << ECh;
      return false;
    }
  }

  return true;
}

void Lexer::lexNumbers(Token &Tok) {
  // numbers
  //
  // currently libexpr accepts:
  // INT         [0-9]+
  // FLOAT       (([1-9][0-9]*\.[0-9]*)|(0?\.[0-9]+))([Ee][+-]?[0-9]+)?
  //
  // regex 'FLOAT' rejects floats like 00.0
  //
  // nix-repl> 000.3
  // error: attempt to call something which is not a function but an integer
  //
  //        at «string»:1:1:
  //
  //             1| 000.3
  //              | ^
  //
  // however, we accept [0-9]+\.[0-9]*([Ee][+-]?[0-9]+)?
  // and issues a warning if it has leading zeros
  std::string NumStr;
  unsigned NumStart = curOffset();
  // [0-9]+
  while (!eof() && isdigit(*Cur)) {
    NumStr += *Cur;
    Cur++;
  }
  if (*Cur == '.') {
    // float
    NumStr += *Cur; // '.'
    Cur++;

    // [0-9]*
    while (!eof() && isdigit(*Cur)) {
      NumStr += *Cur;
      Cur++;
    }

    if (lexFloatExp(NumStr))
      Tok.Kind = tok_float;
    else
      Tok.Kind = tok_err;
    if (NumStr.starts_with("00")) {
      Diags.diag(DK::DK_FloatLeadingZero, {NumStart, NumStart + 2}) << NumStr;
    }
    Tok.Content = std::move(NumStr);
  } else {
    // integer
    Tok.Kind = tok_int;
    Tok.Content = std::move(NumStr);
  }
}

std::shared_ptr<Token> Lexer::lex() {
  // eat leading trivia
  Trivia LT = consumeTrivia();
  auto Tok = std::make_shared<Token>();
  Tok->LeadingTrivia = LT;

  if (eof()) {
    Tok->Kind = tok_eof;
    return Tok;
  }

  if (std::isdigit(*Cur) || *Cur == '.') {
    lexNumbers(*Tok);
    return Tok;
  }

  return Tok;
}
} // namespace nixf
