#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Parse/Parser.h"
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

TEST_F(VLATest, LookupWith) {
  std::shared_ptr<Node> AST = parse("with 1; foo", Diags);
  VariableLookupAnalysis VLA(Diags);
  VLA.runOnAST(*AST);

  const auto &With = *static_cast<const ExprWith *>(AST.get());
  const auto &Var = *static_cast<const ExprVar *>(With.expr());

  ASSERT_EQ(Diags.size(), 0);

  VLAResult Result = VLA.query(Var);

  ASSERT_EQ(Result.Kind, VLAResultKind::FromWith);
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
  ASSERT_EQ(Diags[0].range().lCur().column(), 8);
  ASSERT_EQ(Diags[0].tags().size(), 1);
  ASSERT_EQ(Diags[0].tags()[0], DiagnosticTag::Faded);
}

} // namespace
