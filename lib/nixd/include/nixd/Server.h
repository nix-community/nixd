#pragma once

#include "lspserver/Connection.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Protocol.h"
#include <llvm/Support/JSON.h>

namespace nixd {
/// The server instance, nix-related language features goes here
class Server : public lspserver::LSPServer {

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {
    Registry.addMethod("initialize", this, &Server::onInitialize);
    Registry.addNotification("initialized", this, &Server::onInitialized);
  }

  void onInitialize(const lspserver::InitializeParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onInitialized(const lspserver::InitializedParams &Params){};
};

}; // namespace nixd
