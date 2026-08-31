/// \file
/// \brief This file implements CLI initialized configuration.

#include "nixd/CommandLine/Configuration.h"
#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Configuration.h"
#include "nixd/Support/JSON.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/JSON.h>

#include <string>

using namespace nixd;
using namespace llvm::cl;

namespace {

opt<std::string> DefaultConfigJSON{"config",
                                   desc("JSON-encoded initial configuration"),
                                   init(""), cat(NixdCategory)};

opt<bool> EnableProjectConfig{
    "enable-project-config",
    desc("Load trusted .nixd.json from the initialized workspace root; trusted "
         "project configuration may evaluate Nix and execute formatter "
         "commands"),
    init(false), cat(NixdCategory)};

} // namespace

CommandLineConfiguration nixd::parseCLIConfig(ConfigurationPatch DefaultPatch,
                                              ConfigurationPatch LegacyPatch) {
  CommandLineConfiguration Result{
      .baseConfiguration = overlay(defaultConfiguration(), DefaultPatch),
      .configSpecified = DefaultConfigJSON.getNumOccurrences() != 0,
      .enableProjectConfig = EnableProjectConfig,
  };
  Result.baseConfiguration =
      overlay(std::move(Result.baseConfiguration), LegacyPatch);
  if (!Result.configSpecified)
    return Result;

  Result.baseConfiguration = overlay(
      std::move(Result.baseConfiguration),
      nixd::fromJSON<ConfigurationPatch>(nixd::parse(DefaultConfigJSON)));
  return Result;
}
