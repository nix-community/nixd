#include <gtest/gtest.h>

#include "nixutil.h"

#include "Parser.tab.h"

#include "nixd/Expr/Expr.h"
#include "nixd/Parser/Parser.h"

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

  struct VTy : nixt::RecursiveASTVisitor<VTy> {
    ParseData *Data;
    int VisitedNodes = 0;

    void showPos(const Expr *E) {
      try {
        auto BeginIdx = Data->locations.at(E);
        auto EndIdx = Data->end.at(BeginIdx);
        auto Begin = (*Data->PTable)[BeginIdx];
        auto End = (*Data->PTable)[EndIdx];
        VisitedNodes++;
        if (const auto *Elet = dynamic_cast<const ExprLet *>(E)) {
          // This is toplevel declaration, assert the range is correct
          ASSERT_EQ(Begin.line, 2);
          ASSERT_EQ(Begin.column, 1);
          ASSERT_EQ(End.line, 27);
          ASSERT_EQ(End.column, 2);
        }
      } catch (...) {
      }
    }

    bool traverseExpr(const Expr *E) {
      showPos(E);
      return RecursiveASTVisitor<VTy>::traverseExpr(E);
    }

  } V;

  V.Data = Data.get();
  V.traverseExpr(Data->result);

  // Assert that we visited expected nodes.
  ASSERT_EQ(V.VisitedNodes, 61);
}

TEST(Parser, parse1) {
  auto Data = parse("{ x = 1; }", CanonPath("/"), CanonPath("/"));
}

} // namespace nixd
