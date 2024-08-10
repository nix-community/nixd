/// \file
/// \brief Allow default configuration being passed via CLI

#include "nixd/Controller/Configuration.h"

namespace nixd {

/// \brief Parse the CLI flag and initialize the config nixd::DefaultConfig
nixd::Configuration parseCLIConfig();

} // namespace nixd
