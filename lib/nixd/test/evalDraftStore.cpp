#include <exception>
#include <gtest/gtest.h>

#include "nixd/EvalDraftStore.h"

#include <nix/command-installable-value.hh>
#include <nix/installable-value.hh>

#include <memory>

namespace nixd {

TEST(EvalDraftStore, Evaluation) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool(1);

  std::string VirtualTestPath = "/virtual-path-for-testing.nix";

  EDS->addDraft(VirtualTestPath, "0", R"(
    let x = 1; in x
  )");

  EDS->withEvaluation(Pool, {"--file", VirtualTestPath}, "",
                      [=](const EvalDraftStore::CallbackArg &Arg) {
                        const auto &[Excepts, Result] = Arg;
                        auto Forest = Result->EvalASTForest;
                        auto FooAST = Forest.at(VirtualTestPath);
                        ASSERT_TRUE(
                            FooAST->getValue(FooAST->root()).isTrivial());
                      });
  EDS->addDraft(VirtualTestPath, "", R"(
    let x = 2; in x
  )");
  EDS->withEvaluation(Pool, {"--file", VirtualTestPath}, "",
                      [=](const EvalDraftStore::CallbackArg &Arg) {
                        const auto &[Excepts, Result] = Arg;
                        auto Forest = Result->EvalASTForest;
                        auto FooAST = Forest.at(VirtualTestPath);
                        ASSERT_EQ(FooAST->getValue(FooAST->root()).integer, 2);
                      });
  Pool.join();
}

TEST(EvalDraftStore, SetupLookup) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool;

  std::string VirtualTestPath = "/virtual-path-for-testing.nix";

  EDS->addDraft(VirtualTestPath, "0", R"(
    { x = 1; }
  )");

  EDS->withEvaluation(
      Pool, {"--file", VirtualTestPath}, "",
      [=](const EvalDraftStore::CallbackArg &Arg) {
        const auto &[Excepts, Result] = Arg;
        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at(VirtualTestPath);

        /// Ensure that 'lookupPosition' can be used
        ASSERT_EQ(FooAST->lookupPosition({0, 0}), FooAST->root());
      });
  Pool.join();
}

TEST(EvalDraftStore, IgnoreParseError) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool;

  // ParseError!
  EDS->addDraft("/foo", "0", R"(
    { x = 1;
  )");

  EDS->addDraft("/bar", "0", R"(
    { x = 1; }
  )");

  EDS->withEvaluation(
      Pool, {"--file", "/bar"}, "",
      [=](const EvalDraftStore::CallbackArg &Arg) {
        const auto &[Excepts, Result] = Arg;
        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at("/bar");

        /// Ensure that 'lookupPosition' can be used
        ASSERT_EQ(FooAST->lookupPosition({0, 0}), FooAST->root());
      });
  Pool.join();
}

TEST(EvalDraftStore, CanUsePrevious) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool(1);

  EDS->addDraft("/bar", "0", R"(
    { x = 1; }
  )");

  EDS->withEvaluation(
      Pool, {"--file", "/bar"}, "",
      [=](const EvalDraftStore::CallbackArg &Arg) {
        const auto &[Excepts, Result] = Arg;

        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at("/bar");

        /// Ensure that 'lookupPosition' can be used
        ASSERT_EQ(FooAST->lookupPosition({0, 0}), FooAST->root());
      });

  // Undefined variable, to trigger eval error.
  EDS->addDraft("/foo", "0", R"(
    let x = 1; in y
  )");

  bool CalledEvaluation = false;

  EDS->withEvaluation(
      Pool, {"--file", "/foo"}, "",
      [=, &CalledEvaluation](const EvalDraftStore::CallbackArg &Arg) {
        const auto &[Excepts, Result] = Arg;
        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at("/bar");

        /// Ensure that 'lookupPosition' can be used
        ASSERT_EQ(FooAST->lookupPosition({0, 0}), FooAST->root());

        for (auto Except : Excepts) {
          try {
            if (Except != nullptr) {
              std::rethrow_exception(Except);
            }
          } catch (std::exception &E) {
            E.what();
          }
        }

        CalledEvaluation = true;
      });

  Pool.join();
  ASSERT_TRUE(CalledEvaluation);
}

} // namespace nixd
