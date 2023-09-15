#pragma once

#include "Parser.tab.h"

#include "Lexer.tab.h"

#include "nixd/Syntax/Parser.h"
#include "nixd/Syntax/Parser/Require.h"

namespace nixd::syntax {

void parse(char *Text, size_t Size, ParseData *Data) {
  yyscan_t Scanner;
  yylex_init(&Scanner);
  yy_scan_buffer(Text, Size, Scanner);
  yyparse(Scanner, Data);
  yylex_destroy(Scanner);
}

} // namespace nixd::syntax
