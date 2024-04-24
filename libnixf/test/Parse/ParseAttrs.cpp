#include <gtest/gtest.h>

#include "Parser.h"

#include "nixf/Parse/Parser.h"

namespace {

using namespace nixf;
using namespace std::string_view_literals;

TEST(Parser, AttrsOK) {
  auto Src = R"({})"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 2, 2));
  ASSERT_FALSE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, RecAttrsOK) {
  auto Src = R"(rec { })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 7, 7));
  ASSERT_TRUE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, RecAttrsMissingLCurly) {
  auto Src = R"(rec )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 3, 3));
  ASSERT_TRUE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 2);
  auto &D = Diags;
  ASSERT_TRUE(D[0].range().lCur().isAt(0, 3, 3));
  ASSERT_TRUE(D[0].range().rCur().isAt(0, 3, 3));
  ASSERT_EQ(D[0].kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D[0].args().size(), 1);
  ASSERT_EQ(D[0].args()[0], "{");
  ASSERT_TRUE(D[0].range().lCur().isAt(0, 3, 3));
  ASSERT_TRUE(D[0].range().rCur().isAt(0, 3, 3));
  ASSERT_EQ(D[1].kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D[1].args().size(), 1);
  ASSERT_EQ(D[1].args()[0], "}");

  // Check the note.
  ASSERT_EQ(D[0].notes().size(), 0);
  ASSERT_EQ(D[1].notes().size(), 1);
  const auto &N = D[1].notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().rCur().isAt(0, 3, 3));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "rec");

  // Check fix-it hints.
  ASSERT_EQ(D[0].fixes().size(), 1);
  ASSERT_EQ(D[0].fixes()[0].edits().size(), 1);
  ASSERT_EQ(D[0].fixes()[0].message(), "insert {");
  const auto &F = D[0].fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 3, 3));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 3, 3));
  ASSERT_EQ(F.newText(), "{");

  ASSERT_EQ(D[1].fixes().size(), 1);
  ASSERT_EQ(D[1].fixes()[0].edits().size(), 1);
  ASSERT_EQ(D[1].fixes()[0].message(), "insert }");
  const auto &F2 = D[1].fixes()[0].edits()[0];
  ASSERT_TRUE(F2.oldRange().lCur().isAt(0, 3, 3));
  ASSERT_TRUE(F2.oldRange().rCur().isAt(0, 3, 3));
  ASSERT_EQ(F2.newText(), "}");
}

TEST(Parser, AttrsOrID) {
  auto Src = R"({ or = 1; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 11, 11));
  ASSERT_FALSE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 2, 2));
  ASSERT_TRUE(D.range().rCur().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_OrIdentifier);
}

TEST(Parser, AttrsOKSpecialAttr) {
  auto Src = R"({ a.b."foo".${"bar"} = 1; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 27, 27));

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, AttrsMissingRCurly) {
  auto Src = R"(rec { a = 1; )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 12, 12));
  ASSERT_TRUE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 12, 12));
  ASSERT_TRUE(D.range().rCur().isAt(0, 12, 12));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "}");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 4, 4));
  ASSERT_TRUE(N.range().rCur().isAt(0, 5, 5));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "{");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert }");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 12, 12));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 12, 12));
  ASSERT_EQ(F.newText(), "}");
}

TEST(Parser, AttrsExtraDot) {
  auto Src = R"({ a.b. = 1; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 13, 13));

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 5, 5));
  ASSERT_TRUE(D.range().rCur().isAt(0, 6, 6));
  ASSERT_EQ(D.kind(), Diagnostic::DK_AttrPathExtraDot);

  // Check that the note is correct.
  ASSERT_EQ(D.notes().size(), 0);

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 2);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "remove extra .");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 5, 5));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 6, 6));
  ASSERT_EQ(F.newText(), "");

  ASSERT_EQ(D.fixes()[1].edits().size(), 1);
  ASSERT_EQ(D.fixes()[1].message(), "insert dummy attrname");
  const auto &F2 = D.fixes()[1].edits()[0];
  ASSERT_TRUE(F2.oldRange().lCur().isAt(0, 6, 6));
  ASSERT_TRUE(F2.oldRange().rCur().isAt(0, 6, 6));
  ASSERT_EQ(F2.newText(), "\"dummy\"");
}

TEST(Parser, ExprVar) {
  auto Src = R"(a)"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprVar);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 1, 1));

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, AttrsBinding) {
  auto Src = R"(
{
  a = 1;
  b = 2;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().lCur().isAt(1, 0, 1));
  ASSERT_TRUE(AST->range().rCur().isAt(4, 1, 22));

  ASSERT_EQ(Diags.size(), 0);

  // Check the bindings.
  auto &B = *static_cast<ExprAttrs *>(AST.get());
  assert(B.binds() && "expected bindings");
  ASSERT_EQ(B.binds()->bindings().size(), 2);
  ASSERT_TRUE(B.binds()->bindings()[0]->range().lCur().isAt(2, 2, 5));
  ASSERT_TRUE(B.binds()->bindings()[0]->range().rCur().isAt(2, 8, 11));

  ASSERT_TRUE(B.binds()->bindings()[1]->range().lCur().isAt(3, 2, 14));
  ASSERT_TRUE(B.binds()->bindings()[1]->range().rCur().isAt(3, 8, 20));
}

TEST(Parser, AttrsBindingEarlyExit) {
  auto Src = R"(
{
  a; # Missing =, but not continue parsing "expr"
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 2);
}

TEST(Parser, AttrsBindingInherit) {
  auto Src = R"(
{
  a = 1;
  inherit;
  inherit a;
  inherit a b;
  inherit (a) b;
  inherit (a) a b;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 6);

  // Check the bindings.
  const auto &B = static_cast<ExprAttrs *>(AST.get())->binds()->bindings();
  ASSERT_EQ(B.size(), 6);
  ASSERT_TRUE(B[0]->range().lCur().isAt(2, 2, 5));
  ASSERT_TRUE(B[0]->range().rCur().isAt(2, 8, 11));
  ASSERT_EQ(B[0]->kind(), Node::NK_Binding);

  ASSERT_TRUE(B[1]->range().lCur().isAt(3, 2, 14));
  ASSERT_TRUE(B[1]->range().rCur().isAt(3, 10, 22));
  ASSERT_EQ(B[1]->kind(), Node::NK_Inherit);
  const auto &I = static_cast<Inherit *>(B[1].get());
  ASSERT_EQ(I->names().size(), 0);

  ASSERT_TRUE(B[2]->range().lCur().isAt(4, 2, 25));
  ASSERT_TRUE(B[2]->range().rCur().isAt(4, 12, 35));
  ASSERT_EQ(B[2]->kind(), Node::NK_Inherit);
  const auto &I2 = static_cast<Inherit *>(B[2].get());
  ASSERT_EQ(I2->names().size(), 1);
  ASSERT_TRUE(I2->names()[0]->range().lCur().isAt(4, 10, 33));
  ASSERT_TRUE(I2->names()[0]->range().rCur().isAt(4, 11, 34));
  ASSERT_EQ(I2->names()[0]->kind(), AttrName::ANK_ID);
  ASSERT_EQ(I2->names()[0]->id()->name(), "a");
  ASSERT_EQ(I2->expr(), nullptr);

  ASSERT_TRUE(B[3]->range().lCur().isAt(5, 2, 38));
  ASSERT_TRUE(B[3]->range().rCur().isAt(5, 14, 50));
  ASSERT_EQ(B[3]->kind(), Node::NK_Inherit);
  const auto &I3 = static_cast<Inherit *>(B[3].get());
  ASSERT_EQ(I3->names().size(), 2);
  ASSERT_TRUE(I3->names()[0]->range().lCur().isAt(5, 10, 46));
  ASSERT_TRUE(I3->names()[0]->range().rCur().isAt(5, 11, 47));
  ASSERT_EQ(I3->names()[0]->kind(), AttrName::ANK_ID);
  ASSERT_EQ(I3->names()[0]->id()->name(), "a");
  ASSERT_TRUE(I3->names()[1]->range().lCur().isAt(5, 12, 48));
  ASSERT_TRUE(I3->names()[1]->range().rCur().isAt(5, 13, 49));
  ASSERT_EQ(I3->names()[1]->kind(), AttrName::ANK_ID);
  ASSERT_EQ(I3->names()[1]->id()->name(), "b");
  ASSERT_EQ(I3->expr(), nullptr);
  ASSERT_FALSE(I3->hasExpr());

  ASSERT_TRUE(B[4]->range().lCur().isAt(6, 2, 53));
  ASSERT_TRUE(B[4]->range().rCur().isAt(6, 16, 67));
  ASSERT_EQ(B[4]->kind(), Node::NK_Inherit);
  const auto &I4 = static_cast<Inherit *>(B[4].get());
  ASSERT_EQ(I4->names().size(), 1);
  ASSERT_TRUE(I4->names()[0]->range().lCur().isAt(6, 14, 65));
  ASSERT_TRUE(I4->names()[0]->range().rCur().isAt(6, 15, 66));
  ASSERT_EQ(I4->names()[0]->kind(), AttrName::ANK_ID);
  ASSERT_EQ(I4->names()[0]->id()->name(), "b");
  ASSERT_EQ(I4->expr()->kind(), Node::NK_ExprVar);
  ASSERT_TRUE(I4->hasExpr());

  const auto &I5 = static_cast<Inherit *>(B[5].get());
  ASSERT_EQ(I5->names().size(), 2);
  ASSERT_EQ(I5->names()[0]->id()->name(), "a");
  ASSERT_EQ(I5->names()[1]->id()->name(), "b");
  ASSERT_EQ(I5->expr()->kind(), Node::NK_ExprVar);
  ASSERT_TRUE(I5->hasExpr());
}

TEST(Parser, SyncInherit) {
  auto Src = R"(
{
  inherit (111 a b;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(2, 14, 17));
  ASSERT_TRUE(D.range().rCur().isAt(2, 14, 17));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], ")");
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(2, 10, 13));
  ASSERT_TRUE(N.range().rCur().isAt(2, 11, 14));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "(");
}

TEST(Parser, SyncInherit2) {
  auto Src = R"(
{
  inherit (111 "a" b;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(2, 14, 17));
  ASSERT_TRUE(D.range().rCur().isAt(2, 14, 17));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], ")");
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(2, 10, 13));
  ASSERT_TRUE(N.range().rCur().isAt(2, 11, 14));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "(");
}

TEST(Parser, SyncInherit3) {
  auto Src = R"(
{
  inherit (foo ${a} b;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 2);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(2, 14, 17));
  ASSERT_TRUE(D.range().rCur().isAt(2, 14, 17));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], ")");
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(2, 10, 13));
  ASSERT_TRUE(N.range().rCur().isAt(2, 11, 14));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "(");
}

TEST(Parser, InheritEmpty) {
  auto Src = R"({ inherit; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 2, 2));
  ASSERT_TRUE(D.range().rCur().isAt(0, 9, 9));

  // Check the fix.
  const Fix &F = Diags[0].fixes()[0];

  ASSERT_EQ(F.edits().size(), 2);
  ASSERT_TRUE(F.edits()[0].isRemoval());
  ASSERT_EQ(F.edits()[0].oldRange().lCur().line(), 0);
  ASSERT_EQ(F.edits()[0].oldRange().lCur().column(), 2);
  ASSERT_EQ(F.edits()[0].oldRange().rCur().line(), 0);
  ASSERT_EQ(F.edits()[0].oldRange().rCur().column(), 9);

  // Second, remove the semicolon.
  ASSERT_EQ(F.edits()[1].oldRange().lCur().line(), 0);
  ASSERT_EQ(F.edits()[1].oldRange().lCur().column(), 9);
  ASSERT_EQ(F.edits()[1].oldRange().rCur().line(), 0);
  ASSERT_EQ(F.edits()[1].oldRange().rCur().column(), 10);
}

TEST(Parser, InheritMissingSemi) {
  auto Src = R"(
{
  inherit
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 2);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(2, 9, 12));
  ASSERT_TRUE(D.range().rCur().isAt(2, 9, 12));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], ";");
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(2, 2, 5));
  ASSERT_TRUE(N.range().rCur().isAt(2, 9, 12));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
}

TEST(Parser, SyncAttrs) {
  auto Src = R"(
rec {
  )))
  a )) =  1;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 2);
  const auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(2, 2, 9));
  ASSERT_TRUE(D.range().rCur().isAt(2, 5, 12));
  ASSERT_EQ(D.kind(), Diagnostic::DK_UnexpectedText);
  ASSERT_EQ(D.args().size(), 0);

  const auto &D1 = Diags[1];
  ASSERT_TRUE(D1.range().lCur().isAt(3, 4, 17));
  ASSERT_TRUE(D1.range().rCur().isAt(3, 6, 19));
  ASSERT_EQ(D1.kind(), Diagnostic::DK_UnexpectedText);
  ASSERT_EQ(D1.args().size(), 0);

  // Check the note.
  ASSERT_EQ(D.notes().size(), 0);
}

TEST(Parser, ParseAttrName_StringRange) {
  auto Src = R"("aaa")"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseAttrName();

  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 5, 5));
}

TEST(Parser, ParseAttrName_ExprRange) {
  auto Src = R"(${11})"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseAttrName();

  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 5, 5));
}

} // namespace
