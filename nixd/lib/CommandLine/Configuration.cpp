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

} // namespace

Configuration nixd::parseCLIConfig(ConfigurationPatch DefaultPatch,
                                   ConfigurationPatch LegacyPatch) {
  Configuration Config = overlay(defaultConfiguration(), DefaultPatch);
  Config = overlay(std::move(Config), LegacyPatch);
  if (DefaultConfigJSON.empty())
    return Config;

  return overlay(std::move(Config),
                 nixd::fromJSON<ConfigurationPatch>(nixd::parse(DefaultConfigJSON)));
}
