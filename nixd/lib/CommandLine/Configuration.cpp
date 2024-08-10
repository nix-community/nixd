/// \file
/// \brief This file implements CLI initialized configuration.

#include "nixd/CommandLine/Configuration.h"
#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Configuration.h"
#include "nixd/Support/JSON.h"

#include "lspserver/Logger.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/JSON.h>

#include <iostream>
#include <string>

using namespace nixd;
using namespace llvm::cl;

namespace {

opt<std::string> DefaultConfigJSON{"config",
                                   desc("JSON-encoded initial configuration"),
                                   init(""), cat(NixdCategory)};

} // namespace

Configuration nixd::parseCLIConfig() {
  if (DefaultConfigJSON.empty())
    return {};

  return nixd::fromJSON<Configuration>(nixd::parse(DefaultConfigJSON));
}
