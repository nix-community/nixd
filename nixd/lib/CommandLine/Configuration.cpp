/// \file
/// \brief This file implements CLI initialized configuration.

#include "nixd/CommandLine/Configuration.h"
#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Configuration.h"

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

  llvm::Expected<llvm::json::Value> V = llvm::json::parse(DefaultConfigJSON);

  if (!V) {
    llvm::errs()
        << "The JSON string of default configuration cannot be parsed, reason: "
        << V.takeError() << "\n";
    std::exit(-1);
  }

  llvm::json::Path::Root R;
  Configuration C;
  if (!fromJSON(*V, C, R)) {
    // JSON is valid, but it has incorrect schema
    llvm::errs()
        << "The JSON string of default configuration has invalid schema: "
        << R.getError() << "\n";
    std::exit(-1);
  }
  return C;
}
