#pragma once

#include "nixd/Expr/Expr.h"
#include "nixd/Expr/Nodes.h"

#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/globals.hh>
#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <nix/types.hh>
#include <nix/util.hh>

#include <variant>

// using C a struct allows us to avoid having to define the special
// members that using string_view here would implicitly delete.
struct StringToken {
  const char *p;
  size_t l;
  bool hasIndentation;
  operator std::string_view() const { return {p, l}; }
};

namespace nixd {

struct ParserFormals {
  std::vector<nix::Formal> formals;
  bool ellipsis = false;
};
struct ParseState {
  nix::SymbolTable &symbols;
  nix::PosTable &positions;
};

struct ParseData {
  using IndStringParts = std::vector<
      std::pair<nix::PosIdx, std::variant<nix::Expr *, StringToken>>>;
  using StringParts = std::vector<std::pair<nix::PosIdx, nix::Expr *>>;
  using AttrNames = std::vector<nix::AttrName>;

  std::unique_ptr<nix::SymbolTable> STable;
  std::unique_ptr<nix::PosTable> PTable;

  ParseState state;
  nix::Expr *result;
  nix::SourcePath basePath;
  nix::PosTable::Origin origin;
  std::vector<nix::ErrorInfo> error;
  std::map<nix::PosIdx, nix::PosIdx> end;
  std::map<const void *, nix::PosIdx> locations;

  ASTContext ctx;

  Context<ParserFormals> PFCtx;
  Context<nix::Formal> FCtx;
  Context<nix::Formals> FsCtx;
  Context<nix::AttrPath> APCtx;

  Context<AttrNames> ANCtx;
  Context<StringParts> SPCtx;
  Context<IndStringParts> ISPCtx;
};

} // namespace nixd

#define YY_DECL                                                                \
  int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param, yyscan_t yyscanner,  \
            nixd::ParseData *data)
