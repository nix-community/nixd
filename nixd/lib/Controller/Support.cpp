#include "nixd/Controller/Controller.h"
#include "nixd/Protocol/Protocol.h"
#include "nixd/Support/OwnedRegion.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Bytecode/Write.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/VariableLookup.h"

#include <boost/asio/post.hpp>

#include <mutex>

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
  auto Action = [this, File = std::string(File), Version]() {
    auto Draft = Store.getDraft(File);
    assert(Draft && "Added document is not in the store?");

    std::vector<nixf::Diagnostic> Diagnostics;
    std::shared_ptr<nixf::Node> AST =
        nixf::parse(*Draft->Contents, Diagnostics);

    if (!AST) {
      std::lock_guard G(TUsLock);
      publishDiagnostics(File, Version, Diagnostics);
      TUs.insert_or_assign(File,
                           std::make_shared<NixTU>(std::move(Diagnostics),
                                                   std::move(AST), std::nullopt,
                                                   /*VLA=*/nullptr));
      return;
    }

    auto VLA = std::make_unique<nixf::VariableLookupAnalysis>(Diagnostics);
    VLA->runOnAST(*AST);

    publishDiagnostics(File, Version, Diagnostics);

    // Serialize the AST into shared memory. Prepare for evaluation.
    std::stringstream OS;
    nixf::writeBytecode(OS, AST.get());
    std::string Buf = OS.str();
    if (Buf.empty()) {
      lspserver::log("empty AST for {0}", File);
      std::lock_guard G(TUsLock);
      TUs.insert_or_assign(
          File, std::make_shared<NixTU>(std::move(Diagnostics), std::move(AST),
                                        std::nullopt, std::move(VLA)));
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

    std::lock_guard G(TUsLock);
    TUs.insert_or_assign(
        File, std::make_shared<NixTU>(
                  std::move(Diagnostics), std::move(AST),
                  util::OwnedRegion{std::move(Shm), std::move(Region)},
                  std::move(VLA)));

    if (Eval) {
      Eval->RegisterBC(rpc::RegisterBCParams{
          ShmName, ".", ".", static_cast<std::int64_t>(Buf.size())});
    }
  };
  Action();
}

void Controller::createWorkDoneProgress(
    const lspserver::WorkDoneProgressCreateParams &Params) {
  if (ClientCaps.WorkDoneProgress)
    CreateWorkDoneProgress(Params, [](llvm::Expected<std::nullptr_t> Reply) {
      if (!Reply)
        elog("create workdone progress error: {0}", Reply.takeError());
    });
}

Controller::Controller(std::unique_ptr<lspserver::InboundPort> In,
                       std::unique_ptr<lspserver::OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)), LitTest(false) {

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
  Registry.addMethod("textDocument/definition", this,
                     &Controller::onDefinition);
  Registry.addMethod("textDocument/documentSymbol", this,
                     &Controller::onDocumentSymbol);
  Registry.addMethod("textDocument/semanticTokens/full", this,
                     &Controller::onSemanticTokens);
  Registry.addMethod("textDocument/inlayHint", this, &Controller::onInlayHint);
  Registry.addMethod("textDocument/completion", this,
                     &Controller::onCompletion);
  Registry.addMethod("completionItem/resolve", this,
                     &Controller::onCompletionItemResolve);
  Registry.addMethod("textDocument/references", this,
                     &Controller::onReferences);
  Registry.addMethod("textDocument/documentHighlight", this,
                     &Controller::onDocumentHighlight);
  Registry.addMethod("textDocument/codeAction", this,
                     &Controller::onCodeAction);
  Registry.addMethod("textDocument/hover", this, &Controller::onHover);
  Registry.addMethod("textDocument/rename", this, &Controller::onRename);
  Registry.addMethod("textDocument/prepareRename", this,
                     &Controller::onPrepareRename);

  // Workspace features
  Registry.addNotification("workspace/didChangeConfiguration", this,
                           &Controller::onDidChangeConfiguration);

  WorkspaceConfiguration = mkOutMethod<ConfigurationParams, llvm::json::Value>(
      "workspace/configuration");

  PublishDiagnostic = mkOutNotifiction<PublishDiagnosticsParams>(
      "textDocument/publishDiagnostics");
  CreateWorkDoneProgress =
      mkOutMethod<WorkDoneProgressCreateParams, std::nullptr_t>(
          "window/workDoneProgress/create");
  BeginWorkDoneProgress =
      mkOutNotifiction<ProgressParams<WorkDoneProgressBegin>>("$/progress");
  ReportWorkDoneProgress =
      mkOutNotifiction<ProgressParams<WorkDoneProgressReport>>("$/progress");
  EndWorkDoneProgress =
      mkOutNotifiction<ProgressParams<WorkDoneProgressEnd>>("$/progress");
}

} // namespace nixd
