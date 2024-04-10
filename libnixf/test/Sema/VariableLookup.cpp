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

TEST_F(VLATest, LivenessArg) {
  std::shared_ptr<Node> AST = parse("{ foo }: 1", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_DefinitionNotUsed);
  ASSERT_EQ(Diags[0].tags().size(), 1);
  ASSERT_EQ(Diags[0].tags()[0], DiagnosticTag::Faded);
}

TEST_F(VLATest, LivenessNested) {
  std::shared_ptr<Node> AST = parse("let y = 1; in x: y: x + y", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_DefinitionNotUsed);
  ASSERT_EQ(Diags[0].range().lCur().column(), 4);
  ASSERT_EQ(Diags[0].tags().size(), 1);
  ASSERT_EQ(Diags[0].tags()[0], DiagnosticTag::Faded);
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
  ASSERT_EQ(ID->kind(), Node::NK_Identifer);

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

} // namespace
