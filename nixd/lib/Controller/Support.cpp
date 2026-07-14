#include "nixd/Controller/Controller.h"

#include <nixf/Basic/Diagnostic.h>
#include <nixf/Parse/Parser.h>
#include <nixf/Sema/VariableLookup.h>

#include <boost/asio/post.hpp>

#include <mutex>

using namespace lspserver;
using namespace nixd;

namespace {

/// \brief Suppress false-positive unused-formal warnings for names that the Nix
/// flake evaluator unconditionally injects into the outputs function.
///
/// The evaluator calls outputs as:
///   flake.outputs(inputs // { self = result; })
///
/// Suppression is scoped to the outputs lambda's own formals in flake.nix.
/// Nested lambdas and other files are unaffected.
void suppressFlakeInjectedFormals(const nixf::Node &AST,
                                  std::vector<nixf::Diagnostic> &Diagnostics) {
  if (AST.kind() != nixf::Node::NK_ExprAttrs)
    return;

  const auto &S =
      static_cast<const nixf::ExprAttrs &>(AST).sema().staticAttrs();

  std::set<std::string> Injected{"self"};
  if (auto It = S.find("inputs"); It != S.end())
    if (const auto *V = It->second.value();
        V && V->kind() == nixf::Node::NK_ExprAttrs)
      for (const auto &[Name, _] :
           static_cast<const nixf::ExprAttrs &>(*V).sema().staticAttrs())
        Injected.insert(Name);

  auto It = S.find("outputs");
  if (It == S.end())
    return;
  const auto *V = It->second.value();
  if (!V || V->kind() != nixf::Node::NK_ExprLambda)
    return;
  const auto &L = static_cast<const nixf::ExprLambda &>(*V);
  if (!L.arg() || !L.arg()->formals())
    return;

  const auto FR = L.arg()->formals()->range();
  std::erase_if(Diagnostics, [&](const nixf::Diagnostic &D) {
    return (D.kind() == nixf::Diagnostic::DK_UnusedDefLambdaNoArg_Formal ||
            D.kind() == nixf::Diagnostic::DK_UnusedDefLambdaWithArg_Formal) &&
           !D.args().empty() && Injected.contains(D.args()[0]) &&
           FR.contains(D.range());
  });
}

} // namespace

void Controller::removeDocument(lspserver::PathRef File) {
  Store.removeDraft(File);
  {
    std::lock_guard _(TUsLock);
    TUs.erase(File);
  }
  publishDiagnostics(File, std::nullopt, "", {});
}

void Controller::actOnDocumentAdd(PathRef File,
                                  std::optional<int64_t> Version) {
  auto Action = [this, File = std::string(File), Version]() {
    auto Draft = Store.getDraft(File);
    std::shared_ptr<const std::string> Src = Draft->Contents;
    assert(Draft && "Added document is not in the store?");

    std::vector<nixf::Diagnostic> Diagnostics;
    std::shared_ptr<nixf::Node> AST =
        nixf::parse(*Draft->Contents, Diagnostics);

    if (!AST) {
      std::lock_guard G(TUsLock);
      publishDiagnostics(File, Version, *Src, Diagnostics);
      TUs.insert_or_assign(File,
                           std::make_shared<NixTU>(std::move(Diagnostics),
                                                   std::move(AST), std::nullopt,
                                                   /*VLA=*/nullptr, Src));
      return;
    }

    auto VLA = std::make_unique<nixf::VariableLookupAnalysis>(Diagnostics);
    VLA->runOnAST(*AST);

    if (llvm::sys::path::filename(File) == "flake.nix")
      suppressFlakeInjectedFormals(*AST, Diagnostics);

    publishDiagnostics(File, Version, *Src, Diagnostics);

    {
      std::lock_guard G(TUsLock);
      TUs.insert_or_assign(
          File, std::make_shared<NixTU>(std::move(Diagnostics), std::move(AST),
                                        std::nullopt, std::move(VLA), Src));
      return;
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
    : LSPServer(std::move(In), std::move(Out)) {

  // Life Cycle
  Registry.addMethod("initialize", this, &Controller::onInitialize);
  Registry.addNotification("initialized", this, &Controller::onInitialized);

  Registry.addMethod("shutdown", this, &Controller::onShutdown);

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
  Registry.addMethod("textDocument/foldingRange", this,
                     &Controller::onFoldingRange);
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
  Registry.addMethod("textDocument/documentLink", this,
                     &Controller::onDocumentLink);
  Registry.addMethod("textDocument/codeAction", this,
                     &Controller::onCodeAction);
  Registry.addMethod("codeAction/resolve", this,
                     &Controller::onCodeActionResolve);
  Registry.addMethod("textDocument/hover", this, &Controller::onHover);
  Registry.addMethod("textDocument/formatting", this, &Controller::onFormat);
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
  ShowDocument = mkOutMethod<ShowDocumentParams, ShowDocumentResult>(
      "window/showDocument");
  BeginWorkDoneProgress =
      mkOutNotifiction<ProgressParams<WorkDoneProgressBegin>>("$/progress");
  ReportWorkDoneProgress =
      mkOutNotifiction<ProgressParams<WorkDoneProgressReport>>("$/progress");
  EndWorkDoneProgress =
      mkOutNotifiction<ProgressParams<WorkDoneProgressEnd>>("$/progress");
}
