#pragma once

#include "lspserver/Connection.h"

#include <memory>

namespace nixd {

/// \brief Run controller with input & output ports.
///
/// The function will return after received "exit"
void runController(std::unique_ptr<lspserver::InboundPort> In,
                   std::unique_ptr<lspserver::OutboundPort> Out);

} // namespace nixd
