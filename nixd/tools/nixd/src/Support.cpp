#include "Controller.h"

#include "nixd/util/OwnedRegion.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Bytecode/Write.h"
#include "nixf/Parse/Parser.h"

using namespace lspserver;
using namespace nixd;

namespace bipc = boost::interprocess;

namespace {

std::string getShmName(std::string_view File) {
  std::stringstream SS;
  SS << "nixd-tu-" << getpid() << "-"
     << reinterpret_cast<std::uintptr_t>(File.data());
  return SS.str();
}

} // namespace

namespace nixd {

void Controller::actOnDocumentAdd(PathRef File,
                                  std::optional<int64_t> Version) {
  auto Draft = Store.getDraft(File);
  assert(Draft && "Added document is not in the store?");

  std::vector<nixf::Diagnostic> Diagnostics;
  std::shared_ptr<nixf::Node> AST = nixf::parse(*Draft->Contents, Diagnostics);
  publishDiagnostics(File, Version, Diagnostics);

  if (!AST) {
    TUs[File] = NixTU(std::move(Diagnostics), std::move(AST), std::nullopt);
    return;
  }

  // Serialize the AST into shared memory. Prepare for evaluation.
  std::stringstream OS;
  nixf::writeBytecode(OS, *AST);
  std::string Buf = OS.str();
  if (Buf.empty()) {
    lspserver::log("empty AST for {0}", File);
    TUs[File] = NixTU(std::move(Diagnostics), std::move(AST), std::nullopt);
    return;
  }

  // Create an mmap()-ed region, and write AST byte code there.
  std::string ShmName = getShmName(File);

  auto Shm = std::make_unique<util::AutoRemoveShm>(
      ShmName, static_cast<bipc::offset_t>(Buf.size()));

  auto Region =
      std::make_unique<bipc::mapped_region>(Shm->get(), bipc::read_write);

  std::memcpy(Region->get_address(), Buf.data(), Buf.size());

  lspserver::log("serialized AST {0} to {1}, size: {2}", File, ShmName,
                 Buf.size());

  TUs[File] = NixTU(std::move(Diagnostics), std::move(AST),
                    util::OwnedRegion{std::move(Shm), std::move(Region)});

  if (Eval) {
    Eval->registerBC({ShmName, ".", ".", Buf.size()});
  }
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
