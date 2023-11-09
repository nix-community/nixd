/// TokenKinds.inc, token names.

/// provides: TOK(NAME), for all tokens.

/// note: keyword tokens will be prefixed with kw_ and they are declared in
/// TokenKeywords.inc

// Keywords goes here, they are special identifiers.
#ifdef TOK_KEYWORD

TOK_KEYWORD(if)
TOK_KEYWORD(then)
TOK_KEYWORD(else)
TOK_KEYWORD(assert)
TOK_KEYWORD(with)
TOK_KEYWORD(let)
TOK_KEYWORD(in)
TOK_KEYWORD(rec)
TOK_KEYWORD(inherit)
TOK_KEYWORD(or)

#endif // TOK_KEYWORD

#ifdef TOK
TOK(eof)

// Identifier.
TOK(id)

// Numbers.
TOK(int)
TOK(float)

// Strings.
TOK(dquote)
TOK(string_part)
TOK(string_escape) // escaped string, e.g. \r \n \x \"

TOK(quote2) // ''

// Path.
TOK(path_fragment)
// URI.
TOK(uri)

// Interpolation.
TOK(r_curly)      // {
TOK(dollar_curly) // ${

TOK(ellipsis)   // ...
TOK(comma)      // ,
TOK(dot)        // .
TOK(semi_colon) // ;
TOK(eq)         // =
TOK(l_curly)    // {
TOK(l_paren)    // (
TOK(r_paren)    // )
TOK(l_bracket)  // [
TOK(r_bracket)  // ]

TOK(question) // ?
TOK(at)       // @
TOK(colon)    // :

TOK(unknown)

TOK(path_end) // emtpy token, notifies the parser it should exit parsing path.

TOK(op_not) // !

#endif // TOK

// Binary Ops
#ifdef TOK_BIN_OP

TOK_BIN_OP(impl)   // ->
TOK_BIN_OP(or)     // ||
TOK_BIN_OP(and)    // &&
TOK_BIN_OP(eq)     // ==
TOK_BIN_OP(neq)    // !=
TOK_BIN_OP(lt)     // <
TOK_BIN_OP(gt)     // >
TOK_BIN_OP(le)     // <=
TOK_BIN_OP(ge)     // >=
TOK_BIN_OP(update) // //
TOK_BIN_OP(add)    // +
TOK_BIN_OP(negate) // -
TOK_BIN_OP(mul)    // *
TOK_BIN_OP(div)    // /
TOK_BIN_OP(concat) // ++

#endif // TOK_BIN_OP
