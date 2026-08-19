#pragma once

#include "lspserver/Protocol.h"
#include "nixd/CommandLine/Configuration.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nixd {

struct StartupSelection {
  const Configuration baseConfiguration;
  const std::filesystem::path executionCWD;
  const std::optional<std::string> warning;
};

StartupSelection selectStartup(const CommandLineConfiguration &CLI,
                               const lspserver::InitializeParams &Params,
                               const std::filesystem::path &LaunchCWD);

} // namespace nixd
