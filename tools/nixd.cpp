#include "lspserver/Connection.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "nixd/Server.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <system_error>

int main() {
  std::error_code EC;
  lspserver::log("Server started.");
  nixd::Server Server{std::make_unique<lspserver::InboundPort>(),
                      std::make_unique<lspserver::OutboundPort>()};
  Server.run();
  return 0;
}
