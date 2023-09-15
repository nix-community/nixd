#include "Parser.tab.h"

#include "Lexer.tab.h"

#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"
#include "nixd/Syntax/Parser/Require.h"

#include "llvm/Support/FormatVariadic.h"

using namespace nixd::syntax;

YY_DECL;

/// Convert a yacc location (yylloc) to nix::PosIdx
/// Store in the pos table
static nixd::RangeIdx mkRange(YYLTYPE YL, nix::PosTable &T,
                              const nix::PosTable::Origin &Origin) {
  auto Begin = T.add(Origin, YL.first_line, YL.first_column);
  auto End = T.add(Origin, YL.last_line, YL.last_column);
  return {Begin, End};
}

static nixd::RangeIdx mkRange(YYLTYPE YL, nixd::syntax::ParseData &Data) {
  return mkRange(YL, Data.State.Positions, Data.Origin);
}

void yyerror(YYLTYPE *YL, yyscan_t Scanner, nixd::syntax::ParseData *Data,
             const char *Error) {
  auto Range = mkRange(*YL, *Data);
  Diagnostic Diag;
  Diag.Msg = llvm::formatv("{0}", Error);
  Diag.Kind = Diagnostic::Error;
  Diag.Range = Range;
  Data->Diags.emplace_back(std::move(Diag));
}

template <class T>
T *decorateNode(T *Node, YYLTYPE YL, nixd::syntax::ParseData &Data) {
  Data.Nodes.record(Node);
  Node->Range = mkRange(YL, Data);
  return Node;
}

template <class T>
T *mkBinOp(nixd::syntax::Node *LHS, nixd::syntax::Node *RHS, YYLTYPE YL,
           YYLTYPE OpRange, nixd::syntax::ParseData &Data) {
  auto N = decorateNode<T>(new T, YL, Data);
  N->LHS = LHS;
  N->RHS = RHS;
  N->OpRange = mkRange(OpRange, Data);
  return N;
}
