#include "nixd/Server.h"

#include <nix/shared.hh>

namespace nixd {

void Server::initWorker() {
  assert(Role == ServerRole::Controller &&
         "Must switch from controller's fork!");
  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
  nix::initGC();

  /// Basically communicate with the controller in standard mode. because we do
  /// not support "lit-test" outbound port.
  switchStreamStyle(lspserver::JSONStreamStyle::Standard);
}

} // namespace nixd
