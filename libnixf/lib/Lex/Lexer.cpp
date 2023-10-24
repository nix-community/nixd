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
  if (prefix(Prefix)) {
    Cur += Prefix.length();
    return true;
  }
  return false;
}

bool Lexer::prefix(std::string_view Prefix) {
  // Check [Cur, Cur + Prefix.length)
  if (Cur + Prefix.length() > Src.end())
    return false;
  if (remain().starts_with(Prefix)) {
    return true;
  }
  return false;
}

std::optional<TriviaPiece> Lexer::tryConsumeWhitespaces() {
  if (eof() || !std::isspace(*Cur))
    return std::nullopt;

  const char *Ch = Cur;
  TriviaKind Kind = spaceTriviaKind(*Cur);

  // consume same characters and combine them into a TriviaPiece
  do {
    Cur++;
  } while (Cur != Src.end() && *Cur == *Ch);

  return TriviaPiece(Kind, std::string(Ch, Cur));
}

std::optional<TriviaPiece> Lexer::tryConsumeComments() {
  if (eof())
    return std::nullopt;

  std::string_view Remain = remain();
  const char *BeginPtr = Cur;
  unsigned BeginOffset = curOffset();

  if (consumePrefix("/*")) {
    // consume block comments until we meet '*/'
    constexpr auto Kind = TriviaKind::BlockComment;
    while (true) {
      if (eof()) {
        // there is no '*/' to terminate comments
        OffsetRange R = {curOffset() - 1, curOffset()};
        OffsetRange B = {BeginOffset, BeginOffset + 2};

        Diags.diag(DK::DK_UnterminatedBComment, R)
            .note(NK::NK_BCommentBegin, B);

        // recover
        return TriviaPiece(Kind, std::string(Remain));
      }
      if (!eof(Cur + 1) && consumePrefix("*/"))
        // we found the ending '*/'
        return TriviaPiece(Kind, std::string(BeginPtr, Cur));
      Cur++;
    }
  } else if (consumePrefix("#")) {
    // single line comments, consume blocks until we meet EOF or '\n' or '\r'
    while (true) {
      if (eof() || consumeEOL()) {
        return TriviaPiece(TriviaKind::LineComment, std::string(BeginPtr, Cur));
      }
      Cur++;
    }
  } else {
    return std::nullopt;
  }
}

Trivia Lexer::consumeTrivia() {
  if (eof())
    return Trivia({});

  Trivia::TriviaPieces Pieces;
  while (true) {
    if (std::optional<TriviaPiece> OTP = tryConsumeWhitespaces())
      Pieces.emplace_back(std::move(*OTP));
    else if (std::optional<TriviaPiece> OTP = tryConsumeComments())
      Pieces.emplace_back(std::move(*OTP));
    else
      break;
  }

  return Trivia(Pieces);
}

bool Lexer::lexFloatExp() {
  // accept ([Ee][+-]?[0-9]+)?, the exponential part (after `.` of a float)
  if (!eof() && (*Cur == 'E' || *Cur == 'e')) {
    unsigned EOffset = curOffset();
    char ECh = *Cur;
    Cur++;
    // [+-]?
    if (!eof() && (*Cur == '+' || *Cur == '-')) {
      Cur++;
    }
    // [0-9]+
    if (!eof() && isdigit(*Cur)) {
      do {
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

void Lexer::lexNumbers() {
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
  unsigned NumStart = curOffset();
  // [0-9]+
  while (!eof() && isdigit(*Cur))
    Cur++;
  if (*Cur == '.') {
    // float
    Cur++;

    // [0-9]*
    while (!eof() && isdigit(*Cur))
      Cur++;

    if (lexFloatExp())
      Tok = tok_float;
    else
      Tok = tok_err;

  } else {
    // integer
    Tok = tok_int;
  }

  if (tokStr().starts_with("00") && Tok == tok_float)
    Diags.diag(DK::DK_FloatLeadingZero, {NumStart, NumStart + 2}) << tokStr();
}

TokenView Lexer::lexString() {
  // Accept all characters, except ${, or "
  startToken();
  if (eof()) {
    Tok = tok_eof;
    return finishToken();
  }
  switch (*Cur) {
  case '"':
    Cur++;
    Tok = tok_dquote;
    break;
  case '\\':
    // Consume two characters, for escaping
    // NOTE: we may not want to break out Unicode wchar here, but libexpr does
    // such ignoring
    Cur += 2;
    Tok = tok_string_escape;
    break;
  case '$':
    if (consumePrefix("${")) {
      Tok = tok_dollar_curly;
      break;
    }

    // Otherwise, consider it is a part of string.
    [[fallthrough]];
  default:
    Tok = tok_string_part;
    for (; !eof();) {
      // '\' escape
      if (*Cur == '\\')
        break;
      if (*Cur == '"')
        break;
      // double-$, or \$, escapes ${.
      // We will handle escaping on Sema
      if (consumePrefix("$${"))
        continue;
      // Encountered a string interpolation, stop here
      if (prefix("${"))
        break;
      Cur++;
    }
  }
  return finishToken();
}

TokenView Lexer::lex() {
  // eat leading trivia
  LeadingTrivia = std::make_unique<Trivia>(consumeTrivia());
  startToken();

  if (eof()) {
    Tok = tok_eof;
    return finishToken();
  }

  if (std::isdigit(*Cur) || *Cur == '.') {
    lexNumbers();
    return finishToken();
  }
  return finishToken();
}
} // namespace nixf
