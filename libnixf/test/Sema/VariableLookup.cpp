#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/ParentMap.h"
#include "nixf/Sema/VariableLookup.h"

using namespace nixf;

namespace {

using VLAResult = VariableLookupAnalysis::LookupResult;
using VLAResultKind = VariableLookupAnalysis::LookupResultKind;

struct VLATest : public testing::Test {
  std::vector<Diagnostic> Diags;
};

TEST_F(VLATest, UndefinedVariable) {
  std::shared_ptr<Node> AST = parse("a", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UndefinedVariable);

  ASSERT_EQ(AST->kind(), Node::NK_ExprVar);

  const auto &Var = *static_cast<const ExprVar *>(AST.get());

  VLAResult Result = VLA.query(Var);

  ASSERT_EQ(Result.Kind, VLAResultKind::Undefined);
}

TEST_F(VLATest, DefinedBuiltin) {
  std::shared_ptr<Node> AST = parse("builtins", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(AST->kind(), Node::NK_ExprVar);

  const auto &Var = *static_cast<const ExprVar *>(AST.get());

  VLAResult Result = VLA.query(Var);

  ASSERT_EQ(Result.Kind, VLAResultKind::Defined);
  ASSERT_TRUE(Result.Def->isBuiltin());
}

TEST_F(VLATest, LookupLambda) {
  std::shared_ptr<Node> AST = parse("foo: foo", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);

  const auto &Var = *static_cast<const ExprVar *>(
      static_cast<const ExprLambda *>(AST.get())->body());

  VLAResult Result = VLA.query(Var);

  ASSERT_EQ(Result.Kind, VLAResultKind::Defined);
  ASSERT_FALSE(Result.Def->isBuiltin());
}

TEST_F(VLATest, LookupAttrs) {
  std::shared_ptr<Node> AST = parse("rec { a = 1; y = a; }", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, LookupNonRecursiveAttrs) {
  std::shared_ptr<Node> AST = parse("{ a = 1; y = a; }", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UndefinedVariable);
}

TEST_F(VLATest, LookupLet) {
  std::shared_ptr<Node> AST = parse("let a = 1; b = a; in a + b", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, LookupLet2) {
  std::shared_ptr<Node> AST = parse("let a = 1; b = a; in a", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLet);

  const Expr *Body = static_cast<const ExprLet &>(*AST).expr();
  ASSERT_TRUE(Body);
  ASSERT_EQ(Body->kind(), Node::NK_ExprVar);
  auto Result = VLA.query(*static_cast<const ExprVar *>(Body));
  ASSERT_EQ(Result.Kind, VLAResultKind::Defined);
  ASSERT_EQ(Result.Def->syntax()->lCur().column(), 4);
  ASSERT_EQ(Result.Def->syntax()->rCur().column(), 5);
}

TEST_F(VLATest, LookupWith) {
  std::shared_ptr<Node> AST = parse("with 1; foo", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  const auto &With = *static_cast<const ExprWith *>(AST.get());
  const auto &Var = *static_cast<const ExprVar *>(With.expr());

  ASSERT_EQ(Diags.size(), 0);

  VLAResult Result = VLA.query(Var);

  ASSERT_EQ(Result.Kind, VLAResultKind::FromWith);
  ASSERT_EQ(Result.Def->syntax()->range().lCur().column(), 0);
  ASSERT_EQ(Result.Def->syntax()->range().rCur().column(), 4);
}

TEST_F(VLATest, LivenessRec) {
  std::shared_ptr<Node> AST = parse("rec { x = 1; y = 2; z = 3; }", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_ExtraRecursive);
}

TEST_F(VLATest, LivenessFormal) {
  std::shared_ptr<Node> AST = parse("{ foo }: 1", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UnusedDefLambdaNoArg_Formal);
  ASSERT_EQ(Diags[0].tags().size(), 1);
}

TEST_F(VLATest, LivenessLet) {
  std::shared_ptr<Node> AST = parse("let y = 1; in x: y: x + y", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UnusedDefLet);
  ASSERT_EQ(Diags[0].range().lCur().column(), 4);
  ASSERT_EQ(Diags[0].tags().size(), 1);
}

TEST_F(VLATest, LivenessDupSymbol) {
  std::shared_ptr<Node> AST = parse("x @ {x, ...} : x + 1", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_DuplicatedFormalToArg);
  ASSERT_EQ(Diags[0].range().lCur().column(), 0);
  ASSERT_EQ(Diags[0].tags().size(), 0);
}

TEST_F(VLATest, LivenessArgWithFormal) {
  std::shared_ptr<Node> AST = parse("{ foo }@bar: foo", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UnusedDefLambdaWithArg_Arg);
  ASSERT_EQ(Diags[0].range().lCur().column(), 8);
  ASSERT_EQ(Diags[0].tags().size(), 1);
}

TEST_F(VLATest, LivenessFormalWithArg) {
  std::shared_ptr<Node> AST = parse("{ foo }@bar: bar", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UnusedDefLambdaWithArg_Formal);
  ASSERT_EQ(Diags[0].range().lCur().column(), 2);
  ASSERT_EQ(Diags[0].tags().size(), 1);
  ASSERT_EQ(Diags[0].tags()[0], DiagnosticTag::Faded);
}

TEST_F(VLATest, ToDefAttrs) {
  std::shared_ptr<Node> AST = parse("rec { x = 1; y = x; z = x; }", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ParentMapAnalysis PMA;
  PMA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);

  ASSERT_TRUE(AST);
  const Node *ID = AST->descend({{0, 6}, {0, 6}});
  ID = PMA.upTo(*ID, Node::NK_AttrName);
  ASSERT_EQ(ID->kind(), Node::NK_AttrName);

  const auto *Def = VLA.toDef(*ID);
  ASSERT_TRUE(Def);

  ASSERT_EQ(Def->uses().size(), 2);
}

TEST_F(VLATest, ToDefLambda) {
  std::shared_ptr<Node> AST =
      parse("arg: { foo, bar} : arg + foo + bar", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);

  ASSERT_TRUE(AST);
  const Node *ID = AST->descend({{0, 1}, {0, 1}});
  ASSERT_EQ(ID->kind(), Node::NK_Identifier);

  const auto *Def = VLA.toDef(*ID);
  ASSERT_TRUE(Def);

  ASSERT_EQ(Def->uses().size(), 1);
}

TEST_F(VLATest, ToDefWith) {
  std::shared_ptr<Node> AST = parse("with builtins; [ x y z ]", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);

  ASSERT_TRUE(AST);
  const Node *KwWith = AST->descend({{0, 1}, {0, 1}});

  const auto *Def = VLA.toDef(*KwWith);
  ASSERT_TRUE(Def);

  ASSERT_EQ(Def->uses().size(), 3);
}

TEST_F(VLATest, Env) {
  std::shared_ptr<Node> AST = parse("let x = 1; y = 2; in x", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  const Expr *Body = static_cast<ExprLet &>(*AST).expr();
  ASSERT_TRUE(Body);
  ASSERT_EQ(Body->kind(), Node::NK_ExprVar);

  const EnvNode *Env = VLA.env(Body);
  ASSERT_TRUE(Env);
  ASSERT_TRUE(Env->defs().contains("x"));
  ASSERT_TRUE(Env->defs().contains("y"));
}

TEST_F(VLATest, FormalDef) {
  std::shared_ptr<Node> AST = parse("{ a ? 1, b ? a}: b", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, InheritRec) {
  // Make sure inheirt (expr), the expression is binded to "NewEnv".
  std::shared_ptr<Node> AST =
      parse("rec { inherit (foo) bar; foo.bar = 1; }", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, EmptyLetIn) {
  // Test that emtpy let ... in ... expression is considered.
  // https://github.com/nix-community/nixd/issues/500
  std::shared_ptr<Node> AST = parse("{ config }: let in config", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Issue513) {
  // #513
  const char *Src = R"(
rec {
  a = 1;
  b = rec {
    inherit a;
  };
})";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].range().lCur().line(), 3);
}

TEST_F(VLATest, Issue533) {
  const char *Src = R"(
let ;
in 1
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);
}

TEST_F(VLATest, Issue606_NestedWith) {
  const char *Src = R"(
with builtins;
  with builtins;
    concatLists
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Issue711_CurPos) {
  const char *Src = R"(
__curPos
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Builtins_Needs_Prefix) {
  const char *Src = R"(add)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_PrimOpNeedsPrefix);
  ASSERT_EQ(Diags[0].fixes().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[0].edits().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[0].edits()[0].newText(), "builtins.");
}

TEST_F(VLATest, Builtins_Removable_Prefix) {
  const char *Src = R"(builtins.import)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_PrimOpRemovablePrefix);
  ASSERT_EQ(Diags[0].fixes().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[0].edits().size(), 2);
  ASSERT_EQ(Diags[0].fixes()[0].edits()[0].oldRange().lCur().offset(), 0);
  ASSERT_EQ(Diags[0].fixes()[0].edits()[0].oldRange().rCur().offset(), 8);
  ASSERT_EQ(Diags[0].fixes()[0].edits()[0].newText(), "");
  ASSERT_EQ(Diags[0].fixes()[0].edits()[1].oldRange().lCur().offset(), 8);
  ASSERT_EQ(Diags[0].fixes()[0].edits()[1].oldRange().rCur().offset(), 9);
  ASSERT_EQ(Diags[0].fixes()[0].edits()[1].newText(), "");
}

TEST_F(VLATest, Builtins_Unknown) {
  const char *Src = R"(builtins.importaaaa)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_PrimOpUnknown);
  ASSERT_EQ(Diags[0].fixes().size(), 0);
}

TEST_F(VLATest, PrimOp_Overridden) {
  const char *Src = R"(
  let
    __add = true;
  in __add
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_PrimOpOverridden);
  ASSERT_EQ(Diags[0].fixes().size(), 0);
}

TEST_F(VLATest, Constant_true) {
  const char *Src = R"(true)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Constant_false) {
  const char *Src = R"(false)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Constant_null) {
  const char *Src = R"(null)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Constant_Overridden) {
  const char *Src = R"(
  let
    false = true;
  in false
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_ConstantOverridden);
  ASSERT_EQ(Diags[0].fixes().size(), 0);
}

TEST_F(VLATest, Constant_InBuiltins) {
  const char *Src = R"(builtins.false)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Constant_UnderscoreNotInBuiltins) {
  const char *Src = R"(builtins.__false)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 1);
}

TEST_F(VLATest, Constant_UnderscoreStrippedInBuiltins) {
  const char *Src = R"(builtins.nixPath)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, Issue731) {
  // https://github.com/nix-community/nixd/issues/731
  const char *Src = R"(
let
  e =
    let
    in
    2.718;
in
e
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, PrimOp_Inherit_NoWarning) {
  const char *Src = R"(
  let
    inherit (builtins) functionArgs;
  in functionArgs
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, PrimOp_Inherit_PrefixNoWarning) {
  const char *Src = R"(
  let
    othername = builtins;
    inherit (othername) functionArgs;
  in functionArgs
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, PrimOp_Inherit_MatchWarning_LessThan) {
  const char *Src = R"(
  let
    othername = builtins;
    inherit (othername) __lessThan;
  in __lessThan
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_PrimOpOverridden);
}

TEST_F(VLATest, PrimOp_Inherit_MatchWarning_Map) {
  const char *Src = R"(
  let
    othername = builtins;
    inherit (othername) map;
  in map
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_PrimOpOverridden);
}

TEST_F(VLATest, PrimOp_Inherit_NonVar) {
  const char *Src = R"(
  let
    othername.a = builtins;
    inherit (othername.a) foo;
  in foo
  )";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(VLATest, PrimOp_Override_Namespace) {
  const char *Src = R"({ path, ...}: path)";

  std::shared_ptr<Node> AST = parse(Src, Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);
  ASSERT_EQ(Diags.size(), 0);
}

} // namespace
