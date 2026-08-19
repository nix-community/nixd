#include "nixd/Controller/Configuration.h"
#include "nixd/Support/JSON.h"

#include <gtest/gtest.h>

using namespace nixd;

namespace {

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

  auto Config = overlay(
      std::move(Base),
      fromJSON<ConfigurationPatch>(parse(R"json({
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

  auto Config = overlay(
      std::move(Base),
      fromJSON<ConfigurationPatch>(parse(R"json({
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

} // namespace
