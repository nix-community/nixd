#include "nixd/Controller/Configuration.h"
#include "nixd/Controller/Controller.h"
#include "nixd/Support/JSON.h"

#include "ControllerTestPeer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <semaphore>
#include <thread>

using namespace nixd;

namespace {

class ImmediateWorker final : public ProviderWorker {
public:
  void evaluate(std::string, EvaluationCallback Reply) override { Reply(true); }
  void cancel() override {}
  [[nodiscard]] bool alive() const override { return true; }
};

Configuration apply(llvm::StringRef JSON) {
  return overlay(defaultConfiguration(),
                 fromJSON<ConfigurationPatch>(parse(JSON)));
}

TEST(ConfigurationPatch, AppliesExplicitValuesAndClears) {
  auto Config = apply(R"json({
    "nixpkgs": { "expr": "" },
    "formatting": { "command": [] },
    "diagnostic": { "suppress": [] },
    "options": {
      "darwin": { "expr": "darwinOptions" }
    }
  })json");

  EXPECT_EQ(Config.nixpkgs.expr, "");
  EXPECT_TRUE(Config.formatting.command.empty());
  EXPECT_TRUE(Config.diagnostic.suppress.empty());
  ASSERT_EQ(Config.options.size(), 1U);
  EXPECT_EQ(Config.options.at("darwin").expr, "darwinOptions");
}

TEST(ConfigurationPatch, ReplacesOptionsInsteadOfMergingThem) {
  Configuration Base = defaultConfiguration();
  Base.options["extra"] = {.expr = "extraOptions"};

  auto Config =
      overlay(std::move(Base), fromJSON<ConfigurationPatch>(parse(R"json({
        "options": { "home-manager": { "expr": "hmOptions" } }
      })json")));

  ASSERT_EQ(Config.options.size(), 1U);
  EXPECT_EQ(Config.options.at("home-manager").expr, "hmOptions");
}

TEST(ConfigurationPatch, EmptyOptionsObjectClearsTheFullMap) {
  auto Config = apply(R"json({ "options": {} })json");

  EXPECT_TRUE(Config.options.empty());
}

TEST(ConfigurationPatch, EmptyNestedObjectsAreNoOps) {
  Configuration Base = defaultConfiguration();
  Base.formatting.command = {"alejandra"};
  Base.nixpkgs.expr = "myNixpkgs";
  Base.diagnostic.suppress = {"unused"};

  auto Config =
      overlay(std::move(Base), fromJSON<ConfigurationPatch>(parse(R"json({
        "formatting": {}, "nixpkgs": {}, "diagnostic": {}
      })json")));

  EXPECT_EQ(Config.formatting.command, std::vector<std::string>({"alejandra"}));
  EXPECT_EQ(Config.nixpkgs.expr, "myNixpkgs");
  EXPECT_EQ(Config.diagnostic.suppress, std::vector<std::string>({"unused"}));
}

TEST(ConfigurationPatch, SupportsOnlyNonEmptyFormattingStringShorthand) {
  auto Config = apply(R"json({ "formatting": "nixfmt-rfc-style" })json");

  EXPECT_EQ(Config.formatting.command,
            std::vector<std::string>({"nixfmt-rfc-style"}));
  EXPECT_THROW((void)fromJSON<ConfigurationPatch>(
                   parse(R"json({ "formatting": "" })json")),
               JSONSchemaException);
}

TEST(ConfigurationPatch, IgnoresTopLevelSchemaString) {
  auto Config = apply(R"json({
    "$schema": "https://example.test/nixd.schema.json",
    "formatting": { "command": ["alejandra"] }
  })json");

  EXPECT_EQ(Config.formatting.command, std::vector<std::string>({"alejandra"}));
}

TEST(ConfigurationPatch, RejectsInvalidTopLevelSchemaValues) {
  const char *InvalidPatches[] = {
      R"json({ "$schema": null })json",
      R"json({ "$schema": false })json",
      R"json({ "$schema": 1 })json",
      R"json({ "$schema": {} })json",
  };

  for (const char *Invalid : InvalidPatches)
    EXPECT_THROW((void)fromJSON<ConfigurationPatch>(parse(Invalid)),
                 JSONSchemaException)
        << Invalid;
}

TEST(ConfigurationPatch, RejectsNullUnknownAndInvalidOptionEntries) {
  const char *InvalidPatches[] = {
      R"json({ "unknown": true })json",
      R"json({ "nixpkgs": null })json",
      R"json({ "nixpkgs": { "unknown": true } })json",
      R"json({ "formatting": { "command": null } })json",
      R"json({ "diagnostic": { "suppress": null } })json",
      R"json({ "options": { "nixos": {} } })json",
      R"json({ "options": { "nixos": { "expr": "" } } })json",
      R"json({ "options": { "nixos": { "expr": null } } })json",
      R"json({ "options": { "nixos": { "expr": "x", "extra": 1 } } })json",
  };

  for (const char *Invalid : InvalidPatches)
    EXPECT_THROW((void)fromJSON<ConfigurationPatch>(parse(Invalid)),
                 JSONSchemaException)
        << Invalid;
}

TEST(ConfigurationPatch, ConvertsOnlyEvaluatorFieldsToProviderSpec) {
  Configuration First = defaultConfiguration();
  First.nixpkgs.expr = "nixpkgs-a";
  First.options = {{"nixos", {.expr = "options-a"}}};
  First.formatting.command = {"alejandra"};
  First.diagnostic.suppress = {"unused"};

  const ProviderSpec Spec = providerSpec(First);
  ASSERT_TRUE(Spec.Nixpkgs);
  EXPECT_EQ(*Spec.Nixpkgs, "nixpkgs-a");
  EXPECT_EQ(Spec.Options.at("nixos"), "options-a");

  Configuration NonProviderEdit = First;
  NonProviderEdit.formatting.command = {"nixfmt"};
  NonProviderEdit.diagnostic.suppress.clear();
  EXPECT_EQ(providerSpec(NonProviderEdit).Nixpkgs, Spec.Nixpkgs);
  EXPECT_EQ(providerSpec(NonProviderEdit).Options, Spec.Options);

  First.nixpkgs.expr.clear();
  First.options.clear();
  const ProviderSpec Removed = providerSpec(First);
  EXPECT_FALSE(Removed.Nixpkgs);
  EXPECT_TRUE(Removed.Options.empty());
}

TEST(ConfigurationPublication,
     VisibleConfigurationNeverPrecedesProviderTokenInvalidation) {
  Controller C(std::make_unique<lspserver::InboundPort>(-1),
               std::make_unique<lspserver::OutboundPort>());
  ControllerTestPeer::installProviders(C, [](const ProviderKey &,
                                             const std::filesystem::path &,
                                             ProviderWorker::DeathCallback) {
    return std::make_shared<ImmediateWorker>();
  });

  Configuration Initial = defaultConfiguration();
  Initial.nixpkgs.expr = "old-visible";
  Initial.options = {{"zzzz-target", {.expr = "old-target"}}};
  std::binary_semaphore InitialApplied(0);
  ControllerTestPeer::apply(C, Initial, [&](ProviderApplyResult Result) {
    EXPECT_EQ(Result, ProviderApplyResult::Ready);
    InitialApplied.release();
  });
  ASSERT_TRUE(InitialApplied.try_acquire_for(std::chrono::seconds(2)));
  auto OldToken = ControllerTestPeer::providers(C).acquire(
      ProviderKey::option("zzzz-target"));
  ASSERT_TRUE(OldToken);

  Configuration Replacement = defaultConfiguration();
  Replacement.nixpkgs.expr = "new-visible";
  for (unsigned I = 0; I < 20000; ++I)
    Replacement.options.emplace("option-" + std::to_string(I),
                                Configuration::OptionProvider{.expr = "value"});
  Replacement.options.emplace(
      "zzzz-target", Configuration::OptionProvider{.expr = "new-target"});

  std::atomic<bool> ObserverReady = false;
  std::atomic<bool> ObservedOldTokenAfterPublication = false;
  std::jthread Observer([&] {
    ObserverReady = true;
    while (!ControllerTestPeer::configHasNixpkgsExpression(C, "new-visible"))
      std::this_thread::yield();
    ObservedOldTokenAfterPublication =
        ControllerTestPeer::providers(C).validate(*OldToken);
  });
  while (!ObserverReady)
    std::this_thread::yield();

  ControllerTestPeer::apply(C, std::move(Replacement));
  Observer.join();
  EXPECT_FALSE(ObservedOldTokenAfterPublication);
}

} // namespace
