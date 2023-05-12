#include "lspserver/Connection.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "nixd/Server.h"

#include <nix/eval.hh>
#include <nix/shared.hh>

int main() {

  nix::initNix();
  nix::initGC();

  lspserver::log("Server started.");
  nixd::Server Server{std::make_unique<lspserver::InboundPort>(),
                      std::make_unique<lspserver::OutboundPort>()};
  Server.run();
  return 0;
}
