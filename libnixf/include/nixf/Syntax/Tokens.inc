/// Tokens.inc, merge all tokens under "TOK"

#ifdef TOK
#define TOK_BIN_OP(NAME) TOK(op_##NAME)
#define TOK_KEYWORD(NAME) TOK(kw_##NAME)
#include "TokenKinds.inc"
#undef TOK_KEYWORD
#undef TOK_BIN_OP
#endif
