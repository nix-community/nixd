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

TEST_F(SemaActionTest, mergeAttrSets) {
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

TEST_F(SemaActionTest, lastComma) {
  std::vector<Diagnostic> Diags;
  nixf::parse("{a, b, c,}: a + b + c", Diags);

  ASSERT_EQ(Diags.size(), 0);
}

} // namespace
