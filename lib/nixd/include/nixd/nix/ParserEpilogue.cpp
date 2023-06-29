#pragma once

#include "Parser.tab.h"

#include "Lexer.tab.h"

#include "nixd/Expr.h"

#include <nix/eval.hh>
#include <nix/fetchers.hh>
#include <nix/filetransfer.hh>
#include <nix/flake/flake.hh>
#include <nix/nixexpr.hh>
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

void tryFree(auto *&Ptr, std::set<void *> &Freed) {
  if (Freed.contains(Ptr))
    return;

  delete Ptr;
  Freed.insert(Ptr);
  Ptr = nullptr;
}

void destroyAST(auto *&Root, std::set<void *> &Freed) {
  if (Freed.contains(Root))
    return;
#define TRY_TO_TRAVERSE(CHILD)                                                 \
  do {                                                                         \
    destroyAST(CHILD, Freed);                                                  \
    (CHILD) = nullptr;                                                         \
  } while (false)

#define DEF_TRAVERSE_TYPE(TYPE, CODE)                                          \
  if (auto *T = dynamic_cast<nix::TYPE *>(Root)) {                             \
    { CODE; }                                                                  \
  }
#include "nixd/NixASTTraverse.inc"
#undef TRY_TO_TRAVERSE
#undef DEF_TRAVERSE_TYPE
  if (auto *E = dynamic_cast<nix::ExprConcatStrings *>(Root)) {
    tryFree(E->es, Freed);
  }
  if (auto *E = dynamic_cast<nix::ExprLambda *>(Root)) {
    tryFree(E->formals, Freed);
  }
  tryFree(Root, Freed);
}

ParseData::~ParseData() {
  // Destroy AST nodes;
  std::set<void *> Freed;
  destroyAST(result, Freed);
}

} // namespace nixd
