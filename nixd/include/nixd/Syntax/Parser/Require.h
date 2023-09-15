#pragma once

#include "nixd/Support/GCPool.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"

#include <cstddef>
#include <string_view>

namespace nixd::syntax {

/// For compatibility with official implementation
/// The symbols & positions should be mapped to nix::EvalState::state
/// Or owned by "ParseData"
struct ParseState {
  nix::SymbolTable &Symbols;
  nix::PosTable &Positions;
};

struct ParseData {
  Node *Result;

  ParseState State;

  nix::PosTable::Origin Origin;

  nixd::GCPool<Node> Nodes;

  std::vector<Diagnostic> Diags;
};

// Note: copied from
// https://github.com/NixOS/nix/blob/b99fdcf8dbb38ec0be0e82f65d1d138ec9e89dda/src/libexpr/parser.y#L47C1-L54C3
//
// using C a struct allows us to avoid having to define the special
// members that using string_view here would implicitly delete.
struct StringToken {
  const char *p;
  size_t l;
  bool hasIndentation;
  operator std::string_view() const { return {p, l}; }
};

} // namespace nixd::syntax

#define YY_DECL                                                                \
  int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param, yyscan_t yyscanner,  \
            nixd::syntax::ParseData *Data)
