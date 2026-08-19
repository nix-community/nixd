/// \file
/// \brief Allow default configuration being passed via CLI
#pragma once

#include "nixd/Controller/Configuration.h"

#include <exception>

namespace nixd {

/// \brief Parse the startup JSON patch over defaults and CLI patches.
nixd::Configuration parseCLIConfig(nixd::ConfigurationPatch DefaultPatch,
                                  nixd::ConfigurationPatch LegacyPatch);

} // namespace nixd
