#include "nixd/Controller/EditorConfig.h"
#include "nixd/Support/JSON.h"

#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>

#include <llvm/Support/Error.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nixd {
namespace {

class EditorExecutor {
  boost::asio::io_context Context;

public:
  EditorConfigState::Executor executor() {
    return EditorConfigState::Executor(Context.get_executor());
  }

  void runAll() {
    Context.restart();
    Context.poll();
  }
};

llvm::Expected<llvm::json::Value> response(llvm::StringRef JSON) {
  return parse(JSON);
}

llvm::Expected<llvm::json::Value> responseError() {
  return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                 "configuration failed");
}

Configuration editorBase() {
  Configuration Base = defaultConfiguration();
  Base.formatting.command = {"base-format"};
  Base.nixpkgs.expr = "base-nixpkgs";
  Base.options = {{"base", {.expr = "base-options"}}};
  Base.diagnostic.suppress = {"base-diagnostic"};
  return Base;
}

TEST(EditorConfig, OnlyNewestGenerationCanCommitOrReportFailure) {
  EditorExecutor Executor;
  std::vector<Configuration> Commits;
  std::vector<std::string> Errors;
  EditorConfigState State(
      Executor.executor(), editorBase(),
      [&](Configuration Config) { Commits.push_back(std::move(Config)); },
      [&](std::string Error) { Errors.push_back(std::move(Error)); });

  auto Older = State.issue();
  auto Newer = State.issue();
  ASSERT_TRUE(Older);
  ASSERT_TRUE(Newer);
  State.submit(*Newer, response(R"json([{"nixpkgs":{"expr":"newer"}}])json"));
  State.submit(*Older, response(R"json([{"nixpkgs":{"expr":"older"}}])json"));
  Executor.runAll();

  ASSERT_EQ(Commits.size(), 1U);
  EXPECT_EQ(Commits.back().nixpkgs.expr, "newer");

  auto StaleFailure = State.issue();
  auto Current = State.issue();
  ASSERT_TRUE(StaleFailure);
  ASSERT_TRUE(Current);
  State.submit(*StaleFailure, responseError());
  State.submit(*Current,
               response(R"json([{"formatting":{"command":["fresh"]}}])json"));
  Executor.runAll();

  ASSERT_EQ(Commits.size(), 2U);
  EXPECT_EQ(Commits.back().formatting.command,
            std::vector<std::string>({"fresh"}));
  EXPECT_TRUE(Errors.empty());
}

TEST(EditorConfig, NullAndEmptyObjectResetToImmutableStartupBase) {
  EditorExecutor Executor;
  const Configuration Base = editorBase();
  std::vector<Configuration> Commits;
  EditorConfigState State(Executor.executor(), Base, [&](Configuration Config) {
    Commits.push_back(std::move(Config));
  });

  auto Changed = State.issue();
  ASSERT_TRUE(Changed);
  State.submit(*Changed,
               response(R"json([{"nixpkgs":{"expr":"changed"}}])json"));
  Executor.runAll();
  ASSERT_EQ(Commits.size(), 1U);
  EXPECT_EQ(Commits.back().nixpkgs.expr, "changed");

  auto NullReset = State.issue();
  ASSERT_TRUE(NullReset);
  State.submit(*NullReset, response("[null]"));
  Executor.runAll();
  ASSERT_EQ(Commits.size(), 2U);
  EXPECT_EQ(Commits.back().nixpkgs.expr, Base.nixpkgs.expr);
  EXPECT_EQ(Commits.back().formatting.command, Base.formatting.command);
  EXPECT_EQ(Commits.back().options.at("base").expr, "base-options");

  auto ChangedAgain = State.issue();
  ASSERT_TRUE(ChangedAgain);
  State.submit(*ChangedAgain,
               response(R"json([{"options":{"other":{"expr":"x"}}}])json"));
  Executor.runAll();
  ASSERT_EQ(Commits.size(), 3U);
  ASSERT_TRUE(Commits.back().options.contains("other"));

  auto ObjectReset = State.issue();
  ASSERT_TRUE(ObjectReset);
  State.submit(*ObjectReset, response("[{}]"));
  Executor.runAll();
  ASSERT_EQ(Commits.size(), 4U);
  EXPECT_EQ(Commits.back().nixpkgs.expr, Base.nixpkgs.expr);
  EXPECT_EQ(Commits.back().options.at("base").expr, "base-options");
}

TEST(EditorConfig, InvalidResponsesRetainTheActiveConfiguration) {
  EditorExecutor Executor;
  std::vector<Configuration> Commits;
  std::vector<std::string> Errors;
  EditorConfigState State(
      Executor.executor(), editorBase(),
      [&](Configuration Config) { Commits.push_back(std::move(Config)); },
      [&](std::string Error) { Errors.push_back(std::move(Error)); });

  auto Valid = State.issue();
  ASSERT_TRUE(Valid);
  State.submit(*Valid, response(R"json([{"nixpkgs":{"expr":"active"}}])json"));
  Executor.runAll();
  ASSERT_EQ(Commits.size(), 1U);

  const char *InvalidResponses[] = {
      "[]",
      "[{}, {}]",
      R"json({"nixpkgs":{"expr":"not-an-array"}})json",
      R"json(["malformed-config"] )json",
      R"json([{"options":{"broken":{}}}])json",
  };
  for (const char *Invalid : InvalidResponses) {
    auto Generation = State.issue();
    ASSERT_TRUE(Generation);
    State.submit(*Generation, response(Invalid));
    Executor.runAll();
  }
  auto Failure = State.issue();
  ASSERT_TRUE(Failure);
  State.submit(*Failure, responseError());
  Executor.runAll();

  EXPECT_EQ(Commits.size(), 1U);
  EXPECT_EQ(Commits.back().nixpkgs.expr, "active");
  EXPECT_EQ(Errors.size(), 6U);
}

TEST(EditorConfig, OwnsMovedJSONUntilDeferredParsingCompletes) {
  EditorExecutor Executor;
  std::vector<Configuration> Commits;
  EditorConfigState State(
      Executor.executor(), editorBase(),
      [&](Configuration Config) { Commits.push_back(std::move(Config)); });
  auto Generation = State.issue();
  ASSERT_TRUE(Generation);

  {
    auto Response =
        response(R"json([{"nixpkgs":{"expr":"owned-after-callback"}}])json");
    State.submit(*Generation, std::move(Response));
  }
  EXPECT_TRUE(Commits.empty());
  Executor.runAll();

  ASSERT_EQ(Commits.size(), 1U);
  EXPECT_EQ(Commits.back().nixpkgs.expr, "owned-after-callback");
}

TEST(EditorConfig, ShutdownClosesIssueAndCommitGate) {
  EditorExecutor Executor;
  std::vector<Configuration> Commits;
  EditorConfigState State(
      Executor.executor(), editorBase(),
      [&](Configuration Config) { Commits.push_back(std::move(Config)); });
  auto InFlight = State.issue();
  ASSERT_TRUE(InFlight);

  State.stop();
  EXPECT_FALSE(State.accepting());
  EXPECT_FALSE(State.issue());
  State.submit(*InFlight,
               response(R"json([{"nixpkgs":{"expr":"too-late"}}])json"));
  Executor.runAll();

  EXPECT_TRUE(Commits.empty());
}

} // namespace
} // namespace nixd
