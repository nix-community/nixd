#include <gtest/gtest.h>

#include "nixd/CallbackExpr.h"
#include "nixd/Expr.h"
#include "nixutil.h"

#include <nix/eval.hh>
#include <nix/shared.hh>

namespace nixd {

TEST(Expr, Visitor1) {
  static const char *NixSrc = R"(
let
    pkgs = { a = 1; };
in
with pkgs;
a
  )";
  InitNix Inix;
  struct MyVisitor : nixd::RecursiveASTVisitor<MyVisitor> {
    bool VisitedWith = false;
    bool VisitedLet = false;

    bool visitExprLet(const nix::ExprLet *) {
      VisitedLet = true;
      return true;
    }
    bool visitExprWith(const nix::ExprWith *) {
      VisitedWith = true;
      return true;
    }
  } Visitor;

  auto State = Inix.getDummyState();
  auto *ASTRoot = State->parseExprFromString(NixSrc, "/");
  ASSERT_TRUE(Visitor.traverseExpr(ASTRoot));
  ASSERT_TRUE(Visitor.VisitedWith);
  ASSERT_TRUE(Visitor.VisitedLet);
}

TEST(Expr, Visitor2) {
  static const char *NixSrc = R"(
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
  InitNix N;
  struct MyVisitor : nixd::RecursiveASTVisitor<MyVisitor> {
#define NIX_EXPR(EXPR) bool Visited##EXPR = false;
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *) {                                        \
    Visited##EXPR = true;                                                      \
    return true;                                                               \
  }
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR

  } Visitor;

  auto State = N.getDummyState();
  auto *ASTRoot = State->parseExprFromString(NixSrc, "/");
  ASSERT_TRUE(Visitor.traverseExpr(ASTRoot));
#define NIX_EXPR(EXPR) ASSERT_TRUE(Visitor.Visited##EXPR);
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR
}

TEST(Expr, CallbackRewrite) {
  // Verify that all nodes are converted to our types
  struct CallbackASTChecker : nixd::RecursiveASTVisitor<CallbackASTChecker>{

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *E) {                                       \
    assert(dynamic_cast<const nixd::Callback##EXPR *>(E) &&                    \
           "Must be a callback?");                                             \
    return true;                                                               \
  }
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR
                              } Visitor;

  static const char *NixSrc = R"(
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
  InitNix INix;
  auto MyState = INix.getDummyState();
  auto Cxt = std::make_unique<ASTContext>();
  auto *CallbackExprRoot = rewriteCallback(
      *Cxt,
      [](const nix::Expr *, const nix::EvalState &, const nix::Env &,
         const nix::Value &) {},
      MyState->parseExprFromString(NixSrc, "/"));
  Visitor.traverseExpr(CallbackExprRoot);
}

} // namespace nixd
