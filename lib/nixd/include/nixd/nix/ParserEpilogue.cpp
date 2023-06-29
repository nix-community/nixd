#pragma once

#include "Parser.tab.h"

#include "Lexer.tab.h"

#include <nix/eval.hh>
#include <nix/fetchers.hh>
#include <nix/filetransfer.hh>
#include <nix/flake/flake.hh>
#include <nix/store-api.hh>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace nixd {

using namespace nix;

std::unique_ptr<ParseData> parse(char *text, size_t length, Pos::Origin origin,
                                 const SourcePath &basePath, ParseState state) {
  yyscan_t scanner;
  std::unique_ptr<ParseData> data = std::unique_ptr<ParseData>(new ParseData{
      .state =
          {
              .symbols = state.symbols,
              .positions = state.positions,
          },
      .basePath = std::move(basePath),
      .origin = {origin},
  });

  yylex_init(&scanner);
  yy_scan_buffer(text, length, scanner);
  int res = yyparse(scanner, data.get());
  yylex_destroy(scanner);

  return data; // NRVO
}
} // namespace nixd
