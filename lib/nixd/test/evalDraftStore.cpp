#include <gtest/gtest.h>

#include "nixd/EvalDraftStore.h"

#include <nix/command-installable-value.hh>
#include <nix/installable-value.hh>

#include <memory>

namespace nixd {

TEST(EvalDraftStore, Evaluation) {
  auto EDS = std::make_unique<EvalDraftStore>();

  boost::asio::thread_pool Pool;

  std::string VirtualTestPath = "/virtual-path-for-testing.nix";

  EDS->addDraft(VirtualTestPath, "0", R"(
    let x = 1; in x
  )");

  EDS->withEvaluation(
      Pool, {"--file", VirtualTestPath}, "",
      [=](std::variant<std::exception *,
                       nix::ref<EvalDraftStore::EvaluationResult>>
              EvalResult) {
        nix::ref<EvalDraftStore::EvaluationResult> Result =
            std::get<1>(EvalResult);

        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at(VirtualTestPath);
        ASSERT_TRUE(FooAST->getValue(FooAST->root()).isTrivial());
      });
  Pool.join();
  EDS->addDraft(VirtualTestPath, "", R"(
    let x = 2; in x
  )");
  EDS->withEvaluation(
      Pool, {"--file", VirtualTestPath}, "",
      [=](std::variant<std::exception *,
                       nix::ref<EvalDraftStore::EvaluationResult>>
              EvalResult) {
        nix::ref<EvalDraftStore::EvaluationResult> Result =
            std::get<1>(EvalResult);

        auto Forest = Result->EvalASTForest;
        auto FooAST = Forest.at(VirtualTestPath);
        ASSERT_EQ(FooAST->getValue(FooAST->root()).integer, 2);
      });
  Pool.join();
}

} // namespace nixd
