#include <gtest/gtest.h>

#include "nixutil.h"

#include "Parser.tab.h"

#include "nixd/Expr.h"
#include "nixd/Parser.h"

#include <memory>

namespace nixd {

using namespace nix;

TEST(Parser, Basic) {
  ::nixd::InitNix INix;

  auto State = INix.getDummyState();

  std::string Foo =
      R"(
let
  x = 1;
in
rec {
  integerAnd = 1 && 0;
  floatAdd = 1.5 + 2.0;
  ifElse = if 1 then 1 else 0;
  update = { } // { };
  opEq = 1 == 2;
  opNEq = 1 != 2;
  opImpl = 1 -> 2;
  opNot = ! false;
  opOr = 1 || 2;
  lambda = { }: { };
  someAttrSet = { a = 1; };
  hasAttr = someAttrSet ? "a";
  string = "someString";
  listConcat = [ 1 2 ] ++ [ 3 4 ];
  concat = "a" + "b";
  path = ./.;
  exprDiv = 5 / 3;
  exprPos = __curPos;
  exprAssert = assert true ; "some message";
  select = someAttrSet.a;
  exprWith = with someAttrSet; [ ];
}
)";

  Foo.append("\0\0", 2);

  auto Data = nixd::parse(Foo.data(), Foo.length(), CanonPath("/foo"),
                          CanonPath("/bar"), *State);

  struct ASTDump : nixd::RecursiveASTVisitor<ASTDump> {
    ParseData *Data;
    int Depth = 0;

    bool traverseExpr(const nix::Expr *E) {
      Depth++;
      if (!nixd::RecursiveASTVisitor<ASTDump>::traverseExpr(E))
        return false;
      Depth--;
      return true;
    }

    void showPos(const nix::Expr *E) {
      auto EPtr = static_cast<const void *>(E);
      try {
        auto BeginIdx = Data->locations.at(E);
        auto EndIdx = Data->end.at(BeginIdx);
        auto Begin = Data->state.positions[BeginIdx];
        auto End = Data->state.positions[EndIdx];
        // std::cout << Begin.line << ":" << Begin.column << " " << End.line <<
        // ":"
        //           << End.column;
      } catch (...) {
      }
    }

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *E) {                                       \
    for (int i = 0; i < Depth; i++) {                                          \
      std::cout << " ";                                                        \
    }                                                                          \
    std::cout << #EXPR << ": ";                                                \
    E->show(Data->state.symbols, std::cout);                                   \
    std::cout << " ";                                                          \
    showPos(E);                                                                \
    std::cout << "\n";                                                         \
    return true;                                                               \
  }
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR
  } V;

  V.Data = Data.get();
  V.traverseExpr(Data->result);
}

TEST(Parser, Symbol) {
  ::nixd::InitNix INix;

  auto State = INix.getDummyState();

  std::string Foo =
      R"(
rec {
  integerAnd = 1 && 0;
  floatAdd = 1.5 + 2.0;
  ifElse = if 1 then 1 else 0;
  update = { } // { };
  opEq = 1 == 2;
  opNEq = 1 != 2;
  opImpl = 1 -> 2;
  opNot = ! false;
  opOr = 1 || 2;
  lambda = { }: { };
  someAttrSet = { a = 1; };
  hasAttr = someAttrSet ? "a";
  string = "someString";
  listConcat = [ 1 2 ] ++ [ 3 4 ];
  concat = "a" + "b";
  path = ./.;
  exprDiv = 5 / 3;
  exprPos = __curPos;
  exprAssert = assert true ; "some message";
  select = someAttrSet.a;
  exprWith = with someAttrSet; [ ];
  x.y.z = "what";
}
)";

  Foo.append("\0\0", 2);

  auto Data = nixd::parse(Foo.data(), Foo.length(), CanonPath("/foo"),
                          CanonPath("/bar"), *State);

  if (auto Top = dynamic_cast<nix::ExprAttrs *>(Data->result)) {
    for (auto [Sym, _] : Top->attrs) {
      auto BeginIdx = Data->symbolLocations[Sym];
      auto EndIdx = Data->end[BeginIdx];

      auto Begin = Data->state.positions[BeginIdx];
      auto End = Data->state.positions[EndIdx];
      //   std::cout << Data->state.symbols[Sym] << " ";

      //   std::cout << Begin.line << ":" << Begin.column << " " << End.line <<
      //   ":"
      //             << End.column;

      //   std::cout << "\n";
    }
  } else {
    ASSERT_TRUE(false);
  }
}

} // namespace nixd
