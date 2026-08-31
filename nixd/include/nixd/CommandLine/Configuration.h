/// \file
/// \brief Allow default configuration being passed via CLI
#pragma once

#include "nixd/Controller/Configuration.h"

#include <exception>

namespace nixd {

struct CommandLineConfiguration {
  Configuration baseConfiguration;
  bool configSpecified = false;
  bool enableProjectConfig = false;
};

/// Parse all command-line startup configuration and project-file policy.
CommandLineConfiguration parseCLIConfig(nixd::ConfigurationPatch DefaultPatch,
                                        nixd::ConfigurationPatch LegacyPatch);

} // namespace nixd
