#pragma once

#include <nix/eval.hh>
#include <nix/globals.hh>
#include <nix/nixexpr.hh>
#include <nix/util.hh>

#include <variant>

namespace nixd {

using namespace nix;

struct ParseState {
  SymbolTable &symbols;
  PosTable &positions;
};

struct ParseData {
  ParseState state;
  Expr *result;
  SourcePath basePath;
  PosTable::Origin origin;
  std::vector<ErrorInfo> error;
  std::map<PosIdx, PosIdx> end;
  std::map<const void *, PosIdx> locations;
};

struct ParserFormals {
  std::vector<Formal> formals;
  bool ellipsis = false;
};

} // namespace nixd

// using C a struct allows us to avoid having to define the special
// members that using string_view here would implicitly delete.
struct StringToken {
  const char *p;
  size_t l;
  bool hasIndentation;
  operator std::string_view() const { return {p, l}; }
};

#define YY_DECL                                                                \
  int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param, yyscan_t yyscanner,  \
            nixd::ParseData *data)
