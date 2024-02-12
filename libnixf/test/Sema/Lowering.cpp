#include <gtest/gtest.h>
#include <memory>

#include "Lowering.h"

#include "nixf/Basic/Nodes.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/Lowering.h"

namespace {

using namespace nixf;
using namespace std::literals;

class LoweringTest : public ::testing::Test {
protected:
  std::vector<Diagnostic> Diags;
  Lowering L;

public:
  LoweringTest() : L(Diags, "") {}
};

std::unique_ptr<AttrName> getDynamicName(LexerCursorRange Range = {}) {
  return std::make_unique<AttrName>(
      std::make_unique<Interpolation>(Range, nullptr));
}

std::unique_ptr<AttrName> getStaticName(std::string Name,
                                        LexerCursorRange Range = {}) {
  return std::make_unique<AttrName>(
      std::make_unique<Identifier>(Range, std::move(Name)), Range);
}

TEST_F(LoweringTest, selectOrCreate) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  std::vector<std::unique_ptr<AttrName>> Path;
  Path.emplace_back(getStaticName("a"));
  Path.emplace_back(getStaticName("b"));
  Path.emplace_back(getStaticName("c"));
  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Attr.staticAttrs().count("a"), 1);

  SemaAttrs::AttrBody &Inner = Attr.staticAttrs()["a"];
  ASSERT_TRUE(Inner.attrs());

  SemaAttrs *InnerAttr = Inner.attrs();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().count("b"), 1);
  ASSERT_TRUE(InnerAttr->staticAttrs()["b"].attrs());

  InnerAttr = InnerAttr->staticAttrs()["b"].attrs();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 0);

  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 0);
  ASSERT_EQ(Attr.staticAttrs().count("a"), 1);

  ASSERT_TRUE(Inner.attrs());

  InnerAttr = Inner.attrs();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 1);
  ASSERT_EQ(InnerAttr->staticAttrs().count("b"), 1);
  ASSERT_TRUE(InnerAttr->staticAttrs()["b"].attrs());

  InnerAttr = InnerAttr->staticAttrs()["b"].attrs();
  ASSERT_EQ(InnerAttr->staticAttrs().size(), 0);
}

TEST_F(LoweringTest, selectOrCreateDynamic) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  std::vector<std::unique_ptr<AttrName>> Path;
  auto FirstName = getDynamicName();
  const auto *FirstNamePtr = FirstName.get();
  Path.emplace_back(std::move(FirstName));
  Path.emplace_back(getDynamicName());

  L.selectOrCreate(Attr, Path);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Attr.dynamicAttrs().size(), 1);

  auto &DynamicAttr = Attr.dynamicAttrs().front();

  ASSERT_EQ(DynamicAttr.key().syntax(), FirstNamePtr);
}

TEST_F(LoweringTest, insertAttrDup) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getStaticName("a");
  // Check we can detect duplicated attr.
  Attr.staticAttrs()["a"] = SemaAttrs::AttrBody(/*Inherited=*/false, Name.get(),
                                                std::make_unique<SemaAttrs>(
                                                    /*Syntax=*/nullptr,
                                                    /*Recursive=*/false));
  std::unique_ptr<Evaluable> E(new ExprInt{{}, 1});
  L.insertAttr(Attr, *Name, E.get()); // Duplicated

  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 1);
}

TEST_F(LoweringTest, insertAttrOK) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getStaticName("a");
  // Check we can detect duplicated attr.
  std::unique_ptr<Evaluable> E(new ExprInt{{}, 1});
  L.insertAttr(Attr, *Name, E.get());
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, insertAttrNullptr) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getStaticName("a");
  std::unique_ptr<Evaluable> E(nullptr);
  L.insertAttr(Attr, *Name, E.get());
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, inheritName) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getStaticName("a");

  L.lowerInheritName(Attr, Name.get(), nullptr);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, inheritNameNullptr) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getStaticName("a");

  L.lowerInheritName(Attr, nullptr, nullptr);
  ASSERT_EQ(Attr.staticAttrs().size(), 0);
  ASSERT_EQ(Diags.size(), 0);
}

TEST_F(LoweringTest, inheritNameDynamic) {
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getDynamicName(
      {LexerCursor::unsafeCreate(0, 0, 1), LexerCursor::unsafeCreate(0, 1, 2)});

  L.lowerInheritName(Attr, Name.get(), nullptr);
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
  SemaAttrs Attr(/*Syntax=*/nullptr, /*Recursive=*/false);
  auto Name = getStaticName("a");
  Attr.staticAttrs()["a"] = SemaAttrs::AttrBody(/*Inherited=*/true, Name.get(),
                                                std::make_unique<SemaAttrs>(
                                                    /*Syntax=*/nullptr,
                                                    /*Recursive=*/false));

  L.lowerInheritName(Attr, Name.get(), nullptr);
  ASSERT_EQ(Attr.staticAttrs().size(), 1);
  ASSERT_EQ(Diags.size(), 1);
}

} // namespace
