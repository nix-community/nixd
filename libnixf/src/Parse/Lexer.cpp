#include "Lexer.h"

#include "nixf/Basic/Range.h"

#include <cassert>
#include <cctype>

using namespace nixf;
using namespace tok;

namespace {

bool isUriSchemeChar(char Ch) {
  // These characters are valid URI scheme char.
  return std::isalnum(Ch) || Ch == '+' || Ch == '-' || Ch == '.';
}

bool isUriPathChar(char Ch) {
  // These characters are valid URI path char.
  return std::isalnum(Ch) || Ch == '%' || Ch == '/' || Ch == '?' || Ch == ':' ||
         Ch == '@' || Ch == '&' || Ch == '=' || Ch == '+' || Ch == '$' ||
         Ch == ',' || Ch == '-' || Ch == '_' || Ch == '.' || Ch == '!' ||
         Ch == '~' || Ch == '*' || Ch == '\'';
}

bool isPathChar(char Ch) {
  // These characters are valid path char.
  return std::isdigit(Ch) || std::isalpha(Ch) || Ch == '.' || Ch == '_' ||
         Ch == '-' || Ch == '+';
}

bool isIdentifierChar(char Ch) {
  return std::isdigit(Ch) || std::isalpha(Ch) || Ch == '_' || Ch == '\'' ||
         Ch == '-';
}

} // namespace

using DK = Diagnostic::DiagnosticKind;
using NK = Note::NoteKind;

std::optional<LexerCursorRange> Lexer::consumePrefix(std::string_view Prefix) {
  LexerCursor Begin = cur();
  if (peekPrefix(Prefix)) {
    consume(Prefix.length());
    return LexerCursorRange{Begin, cur()};
  }
  return std::nullopt;
}

std::optional<LexerCursorRange> Lexer::consumeManyOf(std::string_view Chars) {
  if (eof())
    return std::nullopt;
  if (Chars.find(peekUnwrap()) != std::string_view::npos) {
    auto Start = Cur;
    while (!eof() && Chars.find(peekUnwrap()) != std::string_view::npos) {
      consume();
    }
    return LexerCursorRange{Start, Cur};
  }
  return std::nullopt;
}

std::optional<char> Lexer::consumeOneOf(std::string_view Chars) {
  if (eof())
    return std::nullopt;
  if (Chars.find(peekUnwrap()) != std::string_view::npos) {
    char Ret = peekUnwrap();
    consume();
    return Ret;
  }
  return std::nullopt;
}

bool Lexer::consumeOne(char C) {
  if (eof())
    return false;
  if (peek() == C) {
    consume();
    return true;
  }
  return false;
}

std::optional<LexerCursorRange> Lexer::consumeManyPathChar() {
  if (eof())
    return std::nullopt;
  if (auto Ch = peek(); Ch && isPathChar(*Ch)) {
    auto Start = Cur;
    do {
      consume();
      Ch = peek();
    } while (Ch && isPathChar(*Ch));
    return LexerCursorRange{Start, Cur};
  }
  return std::nullopt;
}

bool Lexer::peekPrefix(std::string_view Prefix) {
  if (Cur.Offset + Prefix.length() > Src.length())
    return false;
  if (remain().starts_with(Prefix)) {
    return true;
  }
  return false;
}

bool Lexer::consumeWhitespaces() {
  if (auto Ch = peek(); Ch && !std::isspace(*Ch))
    return false;
  do {
    consume();
  } while (!eof() && std::isspace(peekUnwrap()));
  return true;
}

bool Lexer::consumeComments() {
  if (eof())
    return false;
  if (std::optional<LexerCursorRange> BeginRange = consumePrefix("/*")) {
    // Consume block comments until we meet '*/'
    while (true) {
      if (eof()) {
        // There is no '*/' to terminate comments
        Diagnostic &Diag = Diags.emplace_back(DK::DK_UnterminatedBComment,
                                              LexerCursorRange{cur()});
        Diag.note(NK::NK_BCommentBegin, *BeginRange);
        Diag.fix("insert */").edit(TextEdit::mkInsertion(cur(), "*/"));
        return true;
      }
      if (consumePrefix("*/"))
        // We found the ending '*/'
        return true;
      consume(); // Consume a character (block comment body).
    }
  } else if (consumePrefix("#")) {
    // Single line comments, consume blocks until we meet EOF or '\n' or '\r'
    while (true) {
      if (eof() || consumeEOL()) {
        return true;
      }
      consume(); // Consume a character (single line comment body).
    }
  }
  return false;
}

void Lexer::consumeTrivia() {
  while (true) {
    if (eof())
      return;
    if (consumeWhitespaces() || consumeComments())
      continue;
    return;
  }
}

bool Lexer::lexFloatExp() {
  // accept ([Ee][+-]?[0-9]+)?, the exponential part (after `.` of a float)
  if (std::optional<char> ECh = consumeOneOf("Ee")) {
    // [+-]?
    consumeOneOf("+-");
    // [0-9]+
    if (!consumeManyDigits()) {
      // not matching [0-9]+, error
      Diags.emplace_back(DK::DK_FloatNoExp, curRange()) << std::string(1, *ECh);
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
  // [0-9]+
  auto Ch = consumeManyDigits();
  assert(Ch.has_value() && "lexNumbers() must be called with a digit start");
  if (peek() == '.') {
    // float
    Tok = tok_float;
    consume();
    // [0-9]*
    consumeManyDigits();
    lexFloatExp();
    // Checking that if the float token has leading zeros.
    std::string_view Prefix = Src.substr(Ch->lCur().Offset, 2);
    if (Prefix.starts_with("0") && Prefix != "0.")
      Diags.emplace_back(DK::DK_FloatLeadingZero, *Ch) << std::string(Prefix);
  } else {
    Tok = tok_int;
  }
}

bool Lexer::consumePathStart() {
  // PATH_CHAR   [a-zA-Z0-9\.\_\-\+]
  // PATH        {PATH_CHAR}*(\/{PATH_CHAR}+)+\/?
  // PATH_SEG    {PATH_CHAR}*\/
  //

  // Path, starts with any valid path char, and must contain slashs
  // Here, we look ahead characters, the must be valid path char
  // And also check if it contains a slash.
  LexerCursor Saved = cur();

  bool IsStartedWithDot = peek() == '.';

  // {PATH_CHAR}*
  consumeManyPathChar();

  // Check if there is a slash, and also a path char right after such slash.
  // If so, it is a path_fragment
  if (consumeOne('/')) {
    // Now, check if we are on a normal path char.
    if (auto Ch = peek(); Ch && isPathChar(*Ch))
      return true;
    // Or, look ahead to see if is a dollar curly. ${
    // This should be parsed as path-interpolation.
    if (peekPrefix("${"))
      return true;
    // Or, look back to see if is a './'.
    // This should be parsed as path
    if (IsStartedWithDot)
      return true;
  }

  // Otherwise, it is not a path, restore cursor.
  Cur = Saved;
  return false;
}

bool Lexer::consumeURI() {
  // URI
  // [a-zA-Z][a-zA-Z0-9\+\-\.]*\:[a-zA-Z0-9\%\/\?\:\@\&\=\+\$\,\-\_\.\!\~\*\']+
  //

  LexerCursor Saved = cur();
  // URI, starts with any valid URI scheme char, and must contain a colon
  // Here, we look ahead characters, the must be valid path char
  // And also check if it contains a colon.

  while (!eof() && isUriSchemeChar(peekUnwrap()))
    consume();

  // Check if there is a colon, and also a URI path char right after such colon.
  // If so, it is a uri
  if (!eof() && peekUnwrap() == ':') {
    consume();
    if (!eof() && isUriPathChar(peekUnwrap())) {
      do
        consume();
      while (!eof() && isUriPathChar(peekUnwrap()));
      return true;
    }
  }

  Cur = Saved;
  return false;
}

bool Lexer::consumeSPath() {
  //  \<{PATH_CHAR}+(\/{PATH_CHAR}+)*\>
  LexerCursor Saved = cur();

  if (peek() == '<')
    consume();

  if (!eof() && isPathChar(peekUnwrap())) {
    // {PATH_CHAR}+
    while (!eof() && isPathChar(peekUnwrap()))
      consume();
    // (\/{PATH_CHAR}+)*
    while (true) {
      // \/
      if (peek() == '/') {
        consume();
        // {PATH_CHAR}+
        if (!eof() && isPathChar(peekUnwrap())) {
          while (!eof() && isPathChar(peekUnwrap()))
            consume();
          continue;
        }
      }
      break;
    }
    if (peek() == '>') {
      consume();
      return true;
    }
  }

  Cur = Saved;
  return false;
}

void Lexer::lexIdentifier() {
  // identifier: [a-zA-Z_][a-zA-Z0-9_\'\-]*,
  consume();
  while (!eof() && isIdentifierChar(peekUnwrap()))
    consume();
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
#include "nixf/Basic/TokenKinds.inc"
#undef TOK_KEYWORD
}

Token Lexer::lexPath() {
  // Accept all characters, except ${, or "
  // aaa/b//c
  // Path
  //   PathFragment aaa/  <- lex()
  //   PathFragment b//c  <- lexPath()
  startToken();
  Tok = tok_path_end;
  if (eof()) {
    return finishToken();
  }

  if (consumePrefix("${")) {
    Tok = tok_dollar_curly;
    return finishToken();
  }

  if (isPathChar(peekUnwrap()) || peekUnwrap() == '/') {
    Tok = tok_path_fragment;
    while (!eof() && (isPathChar(peekUnwrap()) || peekUnwrap() == '/')) {
      // Encountered an interpolation, stop here
      if (peekPrefix("${"))
        break;
      consume();
    }
    return finishToken();
  }
  return finishToken();
}

Token Lexer::lexString() {
  // Accept all characters, except ${, or "
  startToken();
  if (eof()) {
    Tok = tok_eof;
    return finishToken();
  }
  switch (peekUnwrap()) {
  case '"':
    consume();
    Tok = tok_dquote;
    break;
  case '\\':
    // Consume two characters, for escaping
    // NOTE: we may not want to break out Unicode wchar here, but libexpr does
    // such ignoring
    consume(2);
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
      if (peekUnwrap() == '\\')
        break;
      if (peekUnwrap() == '"')
        break;
      // double-$, or \$, escapes ${.
      // We will handle escaping on Sema
      if (consumePrefix("$${"))
        continue;
      // Encountered a string interpolation, stop here
      if (peekPrefix("${"))
        break;
      consume();
    }
  }
  return finishToken();
}

Token Lexer::lexIndString() {
  startToken();
  if (eof()) {
    Tok = tok_eof;
    return finishToken();
  }
  if (consumePrefix("''")) {
    Tok = tok_quote2;
    if (consumePrefix("$") || consumePrefix("\\") || consumePrefix("'"))
      Tok = tok_string_escape;
    return finishToken();
  }

  if (consumePrefix("${")) {
    Tok = tok_dollar_curly;
    return finishToken();
  }

  Tok = tok_string_part;
  for (; !eof();) {
    if (peekPrefix("''"))
      break;
    // double-$, or \$, escapes ${.
    // We will handle escaping on Sema
    if (consumePrefix("$${"))
      continue;
    // Encountered a string interpolation, stop here
    if (peekPrefix("${"))
      break;
    consume();
  }
  return finishToken();
}

Token Lexer::lex() {
  // eat leading trivia
  consumeTrivia();
  startToken();

  std::optional<char> Ch = peek();

  if (!Ch) {
    Tok = tok_eof;
    return finishToken();
  }

  // Determine if this is a path, or identifier.
  // a/b (including 1/2) should be considered as a whole path, not (a / b)
  if (isPathChar(*Ch) || *Ch == '/') {
    if (consumePathStart()) {
      // Form a concret token, this is a path part.
      Tok = tok_path_fragment;
      return finishToken();
    }
  }

  // Determine if this is a URI.
  if (std::isalpha(*Ch)) {
    if (consumeURI()) {
      Tok = tok_uri;
      return finishToken();
    }
  }

  if (std::isdigit(*Ch)) {
    lexNumbers();
    return finishToken();
  }

  if (std::isalpha(*Ch) || *Ch == '_') {

    // So, this is not a path/URI, it should be an identifier.
    lexIdentifier();
    Tok = tok_id;
    maybeKW();
    return finishToken();
  }

  if (*Ch == '<') {
    // Perhaps this is an "SPATH".
    // e.g. <nixpkgs>
    // \<{PATH_CHAR}+(\/{PATH_CHAR}+)*\>
    if (consumeSPath()) {
      Tok = tok_spath;
      return finishToken();
    }
  }

  switch (*Ch) {
  case '\'':
    if (consumePrefix("''"))
      Tok = tok_quote2;
    break;
  case '+':
    if (consumePrefix("++")) {
      Tok = tok_op_concat;
    } else {
      consume();
      Tok = tok_op_add;
    }
    break;
  case '-':
    if (consumePrefix("->")) {
      Tok = tok_op_impl;
    } else {
      consume();
      Tok = tok_op_negate;
    }
    break;
  case '*':
    consume();
    Tok = tok_op_mul;
    break;
  case '/':
    if (consumePrefix("//")) {
      Tok = tok_op_update;
    } else {
      consume();
      Tok = tok_op_div;
    }
    break;
  case '|':
    if (consumePrefix("||"))
      Tok = tok_op_or;
    break;
  case '!':
    if (consumePrefix("!=")) {
      Tok = tok_op_neq;
    } else {
      consume();
      Tok = tok_op_not;
    }
    break;
  case '<':
    if (consumePrefix("<=")) {
      Tok = tok_op_le;
    } else {
      consume();
      Tok = tok_op_lt;
    }
    break;
  case '>':
    if (consumePrefix(">=")) {
      Tok = tok_op_ge;
    } else {
      consume();
      Tok = tok_op_gt;
    }
    break;
  case '&':
    if (consumePrefix("&&")) {
      Tok = tok_op_and;
      break;
    }
    break;
  case '"':
    consume();
    Tok = tok_dquote;
    break;
  case '}':
    consume();
    Tok = tok_r_curly;
    break;
  case '.':
    if (consumePrefix("...")) {
      Tok = tok_ellipsis;
      break;
    } else {
      consume();
      Tok = tok_dot;
      break;
    }
  case '@':
    consume();
    Tok = tok_at;
    break;
  case ':':
    consume();
    Tok = tok_colon;
    break;
  case '?':
    consume();
    Tok = tok_question;
    break;
  case ';':
    consume();
    Tok = tok_semi_colon;
    break;
  case '=':
    if (consumePrefix("==")) {
      Tok = tok_op_eq;
      break;
    } else {
      consume();
      Tok = tok_eq;
      break;
    }
  case '{':
    consume();
    Tok = tok_l_curly;
    break;
  case '(':
    consume();
    Tok = tok_l_paren;
    break;
  case ')':
    consume();
    Tok = tok_r_paren;
    break;
  case '[':
    consume();
    Tok = tok_l_bracket;
    break;
  case ']':
    consume();
    Tok = tok_r_bracket;
    break;
  case ',':
    consume();
    Tok = tok_comma;
    break;
  case '$':
    if (consumePrefix("${")) {
      Tok = tok_dollar_curly;
      break;
    }
    break;
  }
  if (Tok == tok_unknown)
    consume();
  return finishToken();
}
