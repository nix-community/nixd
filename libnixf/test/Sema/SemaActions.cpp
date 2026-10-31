#include <gtest/gtest.h>
#include <memory>

#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/SemaActions.h"

namespace {

using namespace nixf;
using namespace std::literals;

class SemaActionTest : public ::testing::Test {
protected:
  std::vector<Diagnostic> Diags;
  Sema L;

public:
  SemaActionTest() : L("", Diags) {}
};

std::shared_ptr<AttrName> getDynamicName(LexerCursorRange Range = {}) {
  return std::make_shared<AttrName>(
      std::make_shared<Interpolation>(Range, nullptr));
}

std::shared_ptr<AttrName> getStaticName(std::string Name,
                                        LexerCursorRange Range = {}) {
  return std::make_shared<AttrName>(
      std::make_shared<Identifier>(Range, std::move(Name)), Range);
}

TEST_F(SemaActionTest, selectOrCreate) {
  SemaAttrs Attr(nullptr);
  std::vector<std::shared_ptr<AttrName>> Path;
  Path.emplace_back(getStaticName("a"));
  Path.emplace_back(getStaticName("b"));
  Path.emplace_back(getStaticName("c"));
  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0U);

  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Attr.staticAttrs().count("a"), 1);

  const auto &Inner = Attr.staticAttrs().at("a");

  ASSERT_EQ(Inner.value()->kind(), Node::NK_ExprAttrs);

  const auto *InnerAttr = &static_cast<ExprAttrs *>(Inner.value())->sema();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().count("b"), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().at("b").value()->kind(),
            Node::NK_ExprAttrs);

  InnerAttr =
      &static_cast<ExprAttrs *>(InnerAttr->staticAttrs().at("b").value())
           ->sema();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 0);

  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Attr.staticAttrs().count("a"), 1);

  ASSERT_EQ(Inner.value()->kind(), Node::NK_ExprAttrs);

  InnerAttr = &static_cast<ExprAttrs *>(Inner.value())->sema();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().count("b"), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().at("b").value()->kind(),
            Node::NK_ExprAttrs);

  InnerAttr =
      &static_cast<ExprAttrs *>(InnerAttr->staticAttrs().at("b").value())
           ->sema();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 0);
}

TEST_F(SemaActionTest, selectOrCreateDynamic) {
  SemaAttrs Attr(nullptr);
  std::vector<std::shared_ptr<AttrName>> Path;
  auto FirstName = getDynamicName();
  Path.emplace_back(FirstName);
  Path.emplace_back(getDynamicName());

  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 1);

  const auto &DynamicAttr = Attr.dynamicAttrs().front();

  ASSERT_EQ(DynamicAttr.key().kind(), Node::NK_AttrName);
}

TEST_F(SemaActionTest, insertAttrDup) {
  auto Name = getStaticName("a");
  // Check we can detect duplicated attr.
  std::map<std::string, Attribute> Attrs;

  Attrs.insert(
      {"a", Attribute(
                /*Key=*/Name,
                /*Value=*/std::make_shared<ExprInt>(LexerCursorRange{}, 1),
                Attribute::AttributeKind::Plain)});

  SemaAttrs A(std::move(Attrs), {}, nullptr);
  std::shared_ptr<ExprInt> E(new ExprInt{{}, 1});
  L.insertAttr(A, std::move(Name), std::move(E),
               Attribute::AttributeKind::Plain); // Duplicated

  ASSERT_EQ(A.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 1);
}

TEST_F(SemaActionTest, insertAttrOK) {
  SemaAttrs SA(nullptr);
  auto Name = getStaticName("a");
  // Check we can detect duplicated attr.
  std::shared_ptr<ExprInt> E(new ExprInt{{}, 1});
  L.insertAttr(SA, std::move(Name), std::move(E),
               Attribute::AttributeKind::Plain); // Duplicated
  ASSERT_EQ(SA.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(SemaActionTest, insertAttrNullptr) {
  SemaAttrs SA(nullptr);
  auto Name = getStaticName("a");
  std::shared_ptr<ExprInt> E(new ExprInt{{}, 1});
  L.insertAttr(SA, std::move(Name), std::move(E),
               Attribute::AttributeKind::Plain); // Duplicated
  ASSERT_EQ(SA.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(SemaActionTest, insertAttrNullptr2) {
  SemaAttrs SA(nullptr);
  auto Name = getDynamicName();
  L.insertAttr(SA, std::move(Name), nullptr, Attribute::AttributeKind::Plain);
  ASSERT_EQ(SA.staticAttrs().size(), 0);
  ASSERT_EQ(SA.dynamicAttrs().size(), 0);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(SemaActionTest, inheritName) {
  SemaAttrs Attr(nullptr);
  auto Name = getStaticName("a");

  L.lowerInheritName(Attr, std::move(Name), nullptr,
                     Attribute::AttributeKind::Inherit);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(SemaActionTest, inheritNameNullptr) {
  SemaAttrs Attr(nullptr);
  auto Name = getStaticName("a");

  L.lowerInheritName(Attr, nullptr, nullptr, Attribute::AttributeKind::Inherit);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(SemaActionTest, inheritNameDynamic) {
  SemaAttrs Attr(nullptr);
  auto Name = getDynamicName(
      {LexerCursor::unsafeCreate(0, 0, 1), LexerCursor::unsafeCreate(0, 1, 2)});

  L.lowerInheritName(Attr, Name, nullptr, Attribute::AttributeKind::Inherit);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Diags.size(), 1);

  const Diagnostic &D = Diags.front();
  ASSERT_EQ(D.kind(), Diagnostic::DK_DynamicInherit);
  ASSERT_TRUE(D.range().lCur().isAt(0, 0, 1));
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes().front().edits().size(), 1);
  ASSERT_TRUE(D.fixes().front().edits().front().isRemoval());

  ASSERT_EQ(D.tags().size(), 1);
  ASSERT_EQ(D.tags().front(), DiagnosticTag::Striked);
}

TEST_F(SemaActionTest, inheritNameDuplicated) {
  auto Name = getStaticName("a");
  std::map<std::string, Attribute> Attrs;

  Attrs.insert(
      {"a", Attribute(
                /*Key=*/Name,
                /*Value=*/std::make_shared<ExprInt>(LexerCursorRange{}, 1),
                Attribute::AttributeKind::Plain)});

  SemaAttrs SA(std::move(Attrs), {}, nullptr);
  L.lowerInheritName(SA, Name, nullptr, Attribute::AttributeKind::Inherit);
  ASSERT_EQ(SA.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 1);
}

TEST_F(SemaActionTest, InheritQuotedNames) {
  const auto Src = R"({ inherit path "other" "a b" "a\nb"; })"sv;
  auto AST = parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(Diags.empty());

  const auto &Attrs = static_cast<const ExprAttrs &>(*AST).sema().staticAttrs();
  ASSERT_EQ(Attrs.size(), 4);
  for (const auto &[Name, Attr] : Attrs) {
    ASSERT_EQ(Attr.kind(), Attribute::AttributeKind::Inherit);
    ASSERT_TRUE(Attr.value());
    ASSERT_EQ(Attr.value()->kind(), Node::NK_ExprVar);
    const auto &Var = static_cast<const ExprVar &>(*Attr.value());
    EXPECT_EQ(Var.id().name(), Name);
    EXPECT_EQ(Var.src(Src), Attr.key().src(Src));
    const auto &Key = static_cast<const AttrName &>(Attr.key());
    EXPECT_EQ(Key.kind(),
              Name == "path" ? AttrName::ANK_ID : AttrName::ANK_String);
  }
  EXPECT_TRUE(Attrs.contains("a\nb"));
}

TEST_F(SemaActionTest, InheritQuotedDuplicate) {
  auto AST = parse(R"({ inherit path "path"; })", Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 1);
  EXPECT_EQ(Diags.front().kind(), Diagnostic::DK_DuplicatedAttrName);
}

TEST_F(SemaActionTest, InheritDynamicNamesRecovery) {
  for (const auto Src : {R"({ inherit before ${name} after; })"sv,
                         R"({ inherit before "${name}" after; })"sv,
                         R"({ inherit (source) before ${name} after; })"sv,
                         R"({ inherit (source) before "${name}" after; })"sv}) {
    SCOPED_TRACE(Src);
    Diags.clear();
    auto AST = parse(Src, Diags);
    ASSERT_TRUE(AST);
    ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
    ASSERT_EQ(Diags.size(), 1);
    EXPECT_EQ(Diags.front().kind(), Diagnostic::DK_DynamicInherit);
    const auto &Attrs = static_cast<const ExprAttrs &>(*AST).sema();
    ASSERT_EQ(Attrs.staticAttrs().size(), 2);
    EXPECT_TRUE(Attrs.staticAttrs().contains("before"));
    EXPECT_TRUE(Attrs.staticAttrs().contains("after"));
    EXPECT_TRUE(Attrs.dynamicAttrs().empty());
  }
}

TEST_F(SemaActionTest, mergeAttrSetsDuplicatedAttr) {
  std::shared_ptr<AttrName> XName =
      getStaticName("a", {LexerCursor::unsafeCreate(1, 1, 1),
                          LexerCursor::unsafeCreate(1, 1, 1)});

  std::shared_ptr<AttrName> YName =
      getStaticName("a", {LexerCursor::unsafeCreate(2, 2, 2),
                          LexerCursor::unsafeCreate(1, 1, 1)});

  std::map<std::string, Attribute> XAttrs;
  std::map<std::string, Attribute> YAttrs;

  XAttrs.insert(
      {"a", Attribute(
                /*Key=*/XName,
                /*Value=*/std::make_shared<ExprInt>(LexerCursorRange{}, 1),
                Attribute::AttributeKind::Plain)});

  YAttrs.insert(
      {"a", Attribute(
                /*Key=*/YName,
                /*Value=*/std::make_shared<ExprInt>(LexerCursorRange{}, 1),
                Attribute::AttributeKind::Plain)});

  SemaAttrs XA(XAttrs, {}, nullptr);
  SemaAttrs YA(YAttrs, {}, nullptr);

  L.mergeAttrSets(XA, YA);

  // Report duplicated!
  ASSERT_EQ(Diags.size(), 1);

  // Check that the diagnostic is pointed on the second name.
  LexerCursorRange Range = Diags[0].range();
  ASSERT_EQ(Range.lCur().line(), 2);
}

TEST_F(SemaActionTest, mergeAttrSetsRecursive) {
  SemaAttrs XA(nullptr), YA(nullptr);
  AttrPath XPath{{}, {getStaticName("a"), getStaticName("b")}, {}};
  AttrPath YPath{{}, {getStaticName("a"), getStaticName("c")}, {}};
  L.addAttr(XA, XPath, std::make_shared<ExprInt>(LexerCursorRange{}, 0));
  L.addAttr(YA, YPath, std::make_shared<ExprInt>(LexerCursorRange{}, 1));

  L.mergeAttrSets(XA, YA);

  ASSERT_TRUE(Diags.empty());
  ASSERT_EQ(XA.staticAttrs().size(), 1);
  const auto *Inner = XA.staticAttrs().at("a").value();
  ASSERT_EQ(Inner->kind(), Node::NK_ExprAttrs);
  const auto &Attrs =
      static_cast<const ExprAttrs *>(Inner)->sema().staticAttrs();
  ASSERT_EQ(Attrs.size(), 2);
  EXPECT_TRUE(Attrs.contains("b"));
  EXPECT_TRUE(Attrs.contains("c"));

  const auto *Source = YA.staticAttrs().at("a").value();
  ASSERT_EQ(Source->kind(), Node::NK_ExprAttrs);
  EXPECT_EQ(static_cast<const ExprAttrs *>(Source)->sema().staticAttrs().size(),
            1);
}

TEST_F(SemaActionTest, mergeAttrSetsNestedShorthand) {
  auto AST = nixf::parse("{ a.b.c = 0; a = { b.d = 1; }; }", Diags);
  ASSERT_TRUE(Diags.empty());
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  const auto *Attrs = static_cast<const ExprAttrs *>(AST.get());
  for (const auto *Name : {"a", "b"}) {
    const auto *Value = Attrs->sema().staticAttrs().at(Name).value();
    ASSERT_EQ(Value->kind(), Node::NK_ExprAttrs);
    Attrs = static_cast<const ExprAttrs *>(Value);
  }
  EXPECT_EQ(Attrs->sema().staticAttrs().size(), 2);
  EXPECT_TRUE(Attrs->sema().staticAttrs().contains("c"));
  EXPECT_TRUE(Attrs->sema().staticAttrs().contains("d"));
}

TEST_F(SemaActionTest, mergeAttrSetsNestedDuplicate) {
  nixf::parse("{ a.b.c = 0;\n a = { b.c = 1; }; }", Diags);
  ASSERT_EQ(Diags.size(), 1);
  const auto &D = Diags.front();
  EXPECT_EQ(D.kind(), Diagnostic::DK_DuplicatedAttrName);
  EXPECT_EQ(D.range().lCur().line(), 1);
  ASSERT_EQ(D.notes().size(), 1);
  EXPECT_EQ(D.notes().front().range().lCur().line(), 0);
}

TEST_F(SemaActionTest, lastComma) {
  std::vector<Diagnostic> Diags;
  nixf::parse("{a, b, c,}: a + b + c", Diags);

  ASSERT_EQ(Diags.size(), 0);
}

} // namespace
