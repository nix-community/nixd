/// TokenKinds.inc, token names.

/// provides: TOK(NAME), for all tokens.

/// note: keyword tokens will be prefixed with kw_ and they are declared in
/// TokenKeywords.inc

#ifdef TOK
TOK(eof)

// Identifier.
TOK(id)
// Keywords goes here, they are special identifiers.
#define TOK_KEYWORD(NAME) TOK(kw_##NAME)
#include "TokenKeywords.inc"
#undef TOK_KEYWORD

// Numbers.
TOK(int)
TOK(float)

// Strings.
TOK(dquote)
TOK(string_part)
TOK(string_escape) // escaped string, e.g. \r \n \x \"

// Path.
TOK(path_fragment)

// Interpolation.
TOK(r_curly)      // {
TOK(dollar_curly) // ${

TOK(dot)        // .
TOK(semi_colon) // ;
TOK(eq)         // =
TOK(l_curly)    // {
TOK(l_paren)    // (
TOK(r_paren)    // )
TOK(l_bracket)  // [
TOK(r_bracket)  // ]
// Errorneous token.
TOK(err)

#endif // TOK
