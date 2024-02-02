#include <gtest/gtest.h>

#include "nixd/Server/EvalDraftStore.h"

#include <nix/command-installable-value.hh>
#include <nix/installable-value.hh>

#include <llvm/ADT/StringRef.h>

#include <exception>
#include <memory>

namespace nixd {

const std::string VirtualTestPath = "/virtual-path-for-testing.nix";

TEST(EvalDraftStore, NoError) {
  auto EDS = std::make_unique<EvalDraftStore>();

  EDS->addDraft(VirtualTestPath, "0", R"(
    let x = 1; in x
  )");

  IValueEvalSession Session;
  Session.parseArgs({"--file", VirtualTestPath});
  auto ILR = EDS->injectFiles(Session.getState());
  Session.eval("");
  // Check that there is exactly 1 EvalAST in injected forest
  ASSERT_EQ(ILR.Forest.size(), 1);

  auto EA = ILR.Forest.at(VirtualTestPath);
  auto ERoot = EA->getValue(EA->root());

  ASSERT_TRUE(ERoot->isTrivial());
  ASSERT_EQ(ERoot->type(), nix::ValueType::nInt);
  ASSERT_EQ(ERoot->integer, 1);
}

TEST(EvalDraftStore, IgnoreParseError) {
  auto EDS = std::make_unique<EvalDraftStore>();
  // ParseError!
  EDS->addDraft("/foo", "0", R"(
    { x = 1;
  )");

  EDS->addDraft("/bar", "0", R"(
    { x = 1; }
  )");

  IValueEvalSession Session;
  Session.parseArgs({"--file", "/bar"});
  auto ILR = EDS->injectFiles(Session.getState());

  auto FooAST = ILR.Forest.at("/bar");

  ASSERT_EQ(FooAST->lookupStart({0, 0}), FooAST->root());
}

TEST(EvalDraftStore, TransferInjectionError) {
  auto EDS = std::make_unique<EvalDraftStore>();

  // ParseError!
  EDS->addDraft("/foo", "0", R"(
    { x = 1;
  )");

  /// Error in bindVars
  EDS->addDraft("/barr", "0", R"(
    let x = 1; in y
  )");

  EDS->addDraft("/evaluable.nix", "0", R"(
    let x = 1; in x
  )");

  IValueEvalSession Session;
  auto ILR = EDS->injectFiles(Session.getState());

  const auto &IErrs = ILR.InjectionErrors;
  ASSERT_EQ(IErrs.size(), 2);
  for (const auto &ErrInfo : IErrs) {
    llvm::StringRef ErrWhat = ErrInfo.Err->what();
    if (ErrInfo.ActiveFile == "/foo") {
      ASSERT_TRUE(ErrWhat.contains("syntax error"));
    } else if (ErrInfo.ActiveFile == "/barr") {
      ASSERT_TRUE(ErrWhat.contains("undefined variable"));
    }
  }
}

} // namespace nixd
