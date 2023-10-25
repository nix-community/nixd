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

  if (consumePrefix("/*")) {
    // consume block comments until we meet '*/'
    constexpr auto Kind = TriviaKind::BlockComment;
    while (true) {
      if (eof()) {
        // there is no '*/' to terminate comments
        OffsetRange R = {Cur - 1, Cur};
        OffsetRange B = {BeginPtr, BeginPtr + 2};

        Diagnostic &Diag = Diags.diag(DK::DK_UnterminatedBComment, R);

        Diag.note(NK::NK_BCommentBegin, B);
        Diag.fix(Fix::mkInsertion(R.Begin, "*/"));

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
  }
  return std::nullopt;
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
    const char *ECh = Cur;
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
      Diags.diag(DK::DK_FloatNoExp, {ECh, ECh + 1}) << ECh;
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
  const char *NumStart = Cur;
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
    Tok = tok_int;
  }

  if (tokStr().starts_with("00") && Tok == tok_float)
    Diags.diag(DK::DK_FloatLeadingZero, {NumStart, NumStart + 2}) << tokStr();
}

static bool isPathChar(char Ch) {
  // These characters are valid path char.
  return std::isdigit(Ch) || std::isalpha(Ch) || Ch == '.' || Ch == '_' ||
         Ch == '-' || Ch == '+';
}

const char *Lexer::checkPathStart() {
  // PATH_CHAR   [a-zA-Z0-9\.\_\-\+]
  // PATH        {PATH_CHAR}*(\/{PATH_CHAR}+)+\/?
  // PATH_SEG    {PATH_CHAR}*\/
  //

  // Path, starts with any valid path char, and must contain slashs
  // Here, we look ahead characters, the must be valid path char
  // And also check if it contains a slash.
  const char *PathCursor = Cur;

  // Skipping any path char.
  while (!eof(PathCursor) && isPathChar(*PathCursor))
    PathCursor++;

  // Check if there is a slash, and also a path char right after such slash.
  // If so, it is a path_fragment
  if (!eof(PathCursor) && *PathCursor == '/') {
    PathCursor++;
    // Now, check if we are on a normal path char.
    if (!eof(PathCursor) && isPathChar(*PathCursor)) {
      return PathCursor;
    }
    // Or, look ahead to see if is a dollar curly. ${
    // This should be parsed as path-interpolation.
    if (!eof(PathCursor + 1) && *PathCursor == '$' &&
        *(PathCursor + 1) == '{') {
      return PathCursor;
    }
  }

  return nullptr;
}

static bool isIdentifierChar(char Ch) {
  return std::isdigit(Ch) || std::isalpha(Ch) || Ch == '_' || Ch == '\'' ||
         Ch == '-';
}

void Lexer::lexIdentifier() {
  // identifier: [a-zA-Z_][a-zA-Z0-9_\'\-]*,
  Cur++;
  while (!eof() && isIdentifierChar(*Cur))
    Cur++;
}

void Lexer::maybeKW() {
  // For complex language this should be done on automaton or hashtable.
  // But actually there are few keywords in nix language, so we just do
  // comparison.
#define TOK_KEYWORD(NAME)                                                      \
  if (tokStr() == #NAME) {                                                     \
    Tok = tok_kw_##NAME;                                                       \
    return;                                                                    \
  }
#include "nixf/Syntax/TokenKeywords.inc"
#undef TOK_KEYWORD
}

TokenView Lexer::lexPath() {
  // Accept all characters, except ${, or "
  // aaa/b//c
  // Path
  //   PathFragment aaa/  <- lex()
  //   PathFragment b//c  <- lexPath()
  LeadingTrivia = std::make_unique<Trivia>(consumeTrivia());
  if (eof()) {
    startToken();
    Tok = tok_eof;
    return finishToken();
  }

  if (*Cur == '$') {
    startToken();
    if (consumePrefix("${")) {
      Tok = tok_dollar_curly;
    }
    return finishToken();
  }

  if (isPathChar(*Cur) || *Cur == '/') {
    startToken();
    Tok = tok_path_fragment;
    while (!eof() && (isPathChar(*Cur) || *Cur == '/')) {
      // Encountered an interpolation, stop here
      if (prefix("${"))
        break;
      Cur++;
    }
    return finishToken();
  }

  return lex();
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

  // Determine if this is a path, or identifier.
  // a/b (including 1/2) should be considered as a whole path, not (a / b)
  if (isPathChar(*Cur)) {
    if (const char *PathCursor = checkPathStart()) {
      // Form a concret token, this is a path part.
      Cur = PathCursor;
      Tok = tok_path_fragment;
      return finishToken();
    }
  }

  if (std::isdigit(*Cur)) {
    lexNumbers();
    return finishToken();
  }

  if (std::isalpha(*Cur) || *Cur == '_') {

    // So, this is not a path, it should be an identifier.
    lexIdentifier();
    Tok = tok_id;
    maybeKW();
    return finishToken();
  }

  switch (*Cur) {
  case '"':
    Cur++;
    Tok = tok_dquote;
    break;
  case '}':
    Cur++;
    Tok = tok_r_curly;
    break;
  case '.':
    Cur++;
    Tok = tok_dot;
    break;
  case ';':
    Cur++;
    Tok = tok_semi_colon;
    break;
  case '=':
    Cur++;
    Tok = tok_eq;
    break;
  case '{':
    Cur++;
    Tok = tok_l_curly;
    break;
  case '(':
    Cur++;
    Tok = tok_l_paren;
    break;
  case ')':
    Cur++;
    Tok = tok_r_paren;
  case '$':
    if (consumePrefix("${")) {
      Tok = tok_dollar_curly;
      break;
    }
    break;
  }
  return finishToken();
}
} // namespace nixf
