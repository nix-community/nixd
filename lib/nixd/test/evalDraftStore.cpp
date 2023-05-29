#include <exception>
#include <gtest/gtest.h>

#include "nixd/EvalDraftStore.h"
#include "llvm/ADT/StringRef.h"

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
                        const auto Result = Arg.Result;
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
                        const auto Result = Arg.Result;
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
        const auto Result = Arg.Result;
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
        const auto Result = Arg.Result;
        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at("/bar");

        /// Ensure that 'lookupPosition' can be used
        ASSERT_EQ(FooAST->lookupPosition({0, 0}), FooAST->root());
      });
  Pool.join();
}

TEST(EvalDraftStore, TransferInjectionError) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool;

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

  EDS->withEvaluation(Pool, {"--file", "/evaluable.nix"}, "",
                      [=](const EvalDraftStore::CallbackArg &Arg) {
                        const auto &IErrs = Arg.InjectionErrors;
                        ASSERT_EQ(IErrs.size(), 2);
                        for (auto &[Err, ErrInfo] : IErrs) {
                          llvm::StringRef ErrWhat = Err->what();
                          if (ErrInfo.ActiveFile == "/foo") {
                            ASSERT_TRUE(ErrWhat.contains("syntax error"));
                          } else if (ErrInfo.ActiveFile == "/barr") {
                            ASSERT_TRUE(ErrWhat.contains("undefined variable"));
                          }
                        }
                      });
  Pool.join();
}

TEST(EvalDraftStore, TransferEvalError) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool;

  // ParseError!
  EDS->addDraft("/foo", "0", R"(
    { x = 1;
  )");

  /// Error in bindVars
  EDS->addDraft("/barr", "0", R"(
    let x = 1; in y
  )");

  /// EvalError, missing lambda formal
  EDS->addDraft("/lambda.nix", "0", R"(
    { a, b }: a + b
  )");

  EDS->addDraft("/evaluable.nix", "0", R"(
    { foo = 1; bar = import ./lambda.nix { a = 1; }; }
  )");

  EDS->withEvaluation(
      Pool, {"--file", "/evaluable.nix"}, "bar",
      [=](const EvalDraftStore::CallbackArg &Arg) {
        const auto &IErrs = Arg.InjectionErrors;
        ASSERT_EQ(IErrs.size(), 2);
        for (auto &[Err, ErrInfo] : IErrs) {
          llvm::StringRef ErrWhat = Err->what();
          if (ErrInfo.ActiveFile == "/foo") {
            ASSERT_TRUE(ErrWhat.contains("syntax error"));
          } else if (ErrInfo.ActiveFile == "/barr") {
            ASSERT_TRUE(ErrWhat.contains("undefined variable"));
          }
        }
        ASSERT_TRUE(Arg.EvalError);
        llvm::StringRef ErrWhat = "";
        try {
          std::rethrow_exception(Arg.EvalError);
        } catch (std::exception &Err) {
          ErrWhat = Err.what();
          std::cout << Err.what() << "\n";
        }
        ASSERT_TRUE(ErrWhat.contains("called without required argument"));
      });
  Pool.join();
}

} // namespace nixd
