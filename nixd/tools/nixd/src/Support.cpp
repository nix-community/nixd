#include "Controller.h"
#include "Convert.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/Lowering.h"

using namespace lspserver;
using namespace nixd;

namespace nixd {

void Controller::actOnDocumentAdd(PathRef File,
                                  std::optional<int64_t> Version) {
  auto Draft = Store.getDraft(File);
  assert(Draft && "Added document is not in the store?");
  std::vector<nixf::Diagnostic> Diagnostics;
  std::unique_ptr<nixf::Node> AST = nixf::parse(*Draft->Contents, Diagnostics);
  nixf::lower(AST.get(), *Draft->Contents, Diagnostics);
  publishDiagnostics(File, Version, Diagnostics);
  TUs[File] = NixTU{std::move(Diagnostics), std::move(AST)};
}

Controller::Controller(std::unique_ptr<lspserver::InboundPort> In,
                       std::unique_ptr<lspserver::OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)) {

  // Life Cycle
  Registry.addMethod("initialize", this, &Controller::onInitialize);
  Registry.addNotification("initialized", this, &Controller::onInitialized);

  // Text Document Synchronization
  Registry.addNotification("textDocument/didOpen", this,
                           &Controller::onDocumentDidOpen);
  Registry.addNotification("textDocument/didChange", this,
                           &Controller::onDocumentDidChange);

  Registry.addNotification("textDocument/didClose", this,
                           &Controller::onDocumentDidClose);

  // Language Features
  Registry.addMethod("textDocument/codeAction", this,
                     &Controller::onCodeAction);
  Registry.addMethod("textDocument/hover", this, &Controller::onHover);
}

} // namespace nixd
