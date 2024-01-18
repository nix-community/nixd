/// \file
/// \brief Controller. The process interacting with users.
#include "nixd-config.h"

#include "lspserver/LSPServer.h"

#include <llvm/Support/JSON.h>

namespace {

using namespace lspserver;
using namespace llvm::json;

class Controller : public LSPServer {
public:
  Controller(std::unique_ptr<InboundPort> In, std::unique_ptr<OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {

    // Life Cycle
    Registry.addMethod("initialize", this, &Controller::onInitialize);
    Registry.addNotification("initialized", this, &Controller::onInitialized);
  }

  void onInitialize( // NOLINT(readability-convert-member-functions-to-static)
      [[maybe_unused]] const InitializeParams &Params, Callback<Value> Reply) {

    Object ServerCaps{
        {},
    };

    Object Result{{
        {"serverInfo",
         Object{
             {"name", "nixd"},
             {"version", NIXD_VERSION},
         }},
        {"capabilities", std::move(ServerCaps)},
    }};

    Reply(std::move(Result));
  }
  void onInitialized([[maybe_unused]] const InitializedParams &Params) {}
};

} // namespace

namespace nixd {
void runController(std::unique_ptr<InboundPort> In,
                   std::unique_ptr<OutboundPort> Out) {
  Controller C(std::move(In), std::move(Out));
  C.run();
}
} // namespace nixd
