#include <gtest/gtest.h>
#include <memory>

#include "Lowering.h"

#include "nixf/Basic/Nodes.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/Lowering.h"

namespace {

using namespace nixf;
using namespace std::literals;

class LoweringTest : public ::testing::Test {
protected:
  std::vector<Diagnostic> Diags;
  std::map<nixf::Node *, nixf::Node *> Map;
  Lowering L;

public:
  LoweringTest() : L(Diags, "", Map) {}
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

TEST_F(LoweringTest, selectOrCreate) {
  ExprSemaAttrs Attr({}, /*Recursive=*/false);
  std::vector<std::shared_ptr<AttrName>> Path;
  Path.emplace_back(getStaticName("a"));
  Path.emplace_back(getStaticName("b"));
  Path.emplace_back(getStaticName("c"));
  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Attr.staticAttrs().count("a"), 1);

  auto &Inner = Attr.staticAttrs()["a"];

  ASSERT_EQ(Inner.value()->kind(), Node::NK_ExprSemaAttrs);

  auto *InnerAttr = static_cast<ExprSemaAttrs *>(Inner.value().get());
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().count("b"), 1);
  ASSERT_EQ(InnerAttr->staticAttrs()["b"].value()->kind(),
            Node::NK_ExprSemaAttrs);

  ASSERT_EQ(InnerAttr->staticAttrs()["b"].value()->kind(),
            Node::NK_ExprSemaAttrs);
  InnerAttr =
      static_cast<ExprSemaAttrs *>(InnerAttr->staticAttrs()["b"].value().get());
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 0);

  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Attr.staticAttrs().count("a"), 1);

  ASSERT_EQ(Inner.value()->kind(), Node::NK_ExprSemaAttrs);

  InnerAttr = static_cast<ExprSemaAttrs *>(Inner.value().get());
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().count("b"), 1);
  ASSERT_EQ(InnerAttr->staticAttrs()["b"].value()->kind(),
            Node::NK_ExprSemaAttrs);

  InnerAttr =
      static_cast<ExprSemaAttrs *>(InnerAttr->staticAttrs()["b"].value().get());
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 0);
}

TEST_F(LoweringTest, selectOrCreateDynamic) {
  ExprSemaAttrs Attr({}, /*Recursive=*/false);
  std::vector<std::shared_ptr<AttrName>> Path;
  auto FirstName = getDynamicName();
  Path.emplace_back(FirstName);
  Path.emplace_back(getDynamicName());

  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 1);

  auto &DynamicAttr = Attr.dynamicAttrs().front();

  ASSERT_EQ(DynamicAttr.key()->kind(), Node::NK_AttrName);
}

TEST_F(LoweringTest, insertAttrDup) {
  ExprSemaAttrs A({}, /*Recursive=*/false);
  auto Name = getStaticName("a");
  // Check we can detect duplicated attr.
  A.staticAttrs()["a"] = Attr(
      /*Key=*/Name, /*Value=*/std::make_shared<ExprInt>(LexerCursorRange{}, 1),
      /*ComeFromInherit=*/false);
  std::shared_ptr<ExprInt> E(new ExprInt{{}, 1});
  L.insertAttr(A, std::move(Name), std::move(E), false); // Duplicated

  ASSERT_EQ(A.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 1);
}

TEST_F(LoweringTest, insertAttrOK) {
  ExprSemaAttrs SA({}, /*Recursive=*/false);
  auto Name = getStaticName("a");
  // Check we can detect duplicated attr.
  std::shared_ptr<ExprInt> E(new ExprInt{{}, 1});
  L.insertAttr(SA, std::move(Name), std::move(E),
               /*IsInherit=*/false); // Duplicated
  ASSERT_EQ(SA.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, insertAttrNullptr) {
  ExprSemaAttrs SA({}, /*Recursive=*/false);
  auto Name = getStaticName("a");
  std::shared_ptr<ExprInt> E(new ExprInt{{}, 1});
  L.insertAttr(SA, std::move(Name), std::move(E),
               /*IsInherit=*/false); // Duplicated
  ASSERT_EQ(SA.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, inheritName) {
  ExprSemaAttrs Attr({}, /*Recursive=*/false);
  auto Name = getStaticName("a");

  L.lowerInheritName(Attr, std::move(Name), nullptr);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, inheritNameNullptr) {
  ExprSemaAttrs Attr({}, /*Recursive=*/false);
  auto Name = getStaticName("a");

  L.lowerInheritName(Attr, nullptr, nullptr);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, inheritNameDynamic) {
  ExprSemaAttrs Attr({}, /*Recursive=*/false);
  auto Name = getDynamicName(
      {LexerCursor::unsafeCreate(0, 0, 1), LexerCursor::unsafeCreate(0, 1, 2)});

  L.lowerInheritName(Attr, Name, nullptr);
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

TEST_F(LoweringTest, inheritNameDuplicated) {
  ExprSemaAttrs SA({}, /*Recursive=*/false);
  auto Name = getStaticName("a");
  SA.staticAttrs()["a"] = Attr(
      /*Key=*/Name, /*Value=*/std::make_shared<ExprInt>(LexerCursorRange{}, 1),
      /*ComeFromInherit=*/false);

  L.lowerInheritName(SA, Name, nullptr);
  ASSERT_EQ(SA.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 1);
}

} // namespace
