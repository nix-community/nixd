#include "nixd/Server/Server.h"

namespace nixd {

void Server::switchToStatic() {
  initWorker();
  Role = ServerRole::Static;
  EvalDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");

  Registry.addMethod("nixd/ipc/textDocument/documentLink", this,
                     &Server::onStaticDocumentLink);
  Registry.addMethod("nixd/ipc/textDocument/documentSymbol", this,
                     &Server::onStaticDocumentSymbol);

  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &Server::onStaticCompletion);

  Registry.addMethod("nixd/ipc/textDocument/definition", this,
                     &Server::onStaticDefinition);

  evalInstallable(Config.getEvalDepth());
  mkOutNotifiction<ipc::WorkerMessage>("nixd/ipc/finished")(
      ipc::WorkerMessage{WorkspaceVersion});
}

void Server::onStaticDocumentSymbol(
    const lspserver::TextDocumentIdentifier &Params,
    lspserver::Callback<std::vector<lspserver::DocumentSymbol>> Reply) {
  using Symbols = std::vector<lspserver::DocumentSymbol>;
  using namespace lspserver;
  withAST<Symbols>(Params.uri.file().str(),
                   ReplyRAII<Symbols>(std::move(Reply)),
                   [Params, File = Params.uri.file().str(),
                    State = IER->Session->getState()](
                       const nix::ref<EvalAST> &AST, ReplyRAII<Symbols> &&RR) {
                     RR.Response = AST->documentSymbol(State->symbols);
                   });
}

void Server::onStaticDocumentLink(
    const lspserver::TextDocumentIdentifier &Params,
    lspserver::Callback<std::vector<lspserver::DocumentLink>> Reply) {
  using Links = std::vector<lspserver::DocumentLink>;
  using namespace lspserver;
  withAST<Links>(Params.uri.file().str(), ReplyRAII<Links>(std::move(Reply)),
                 [Params, File = Params.uri.file().str()](
                     const nix::ref<EvalAST> &AST, ReplyRAII<Links> &&RR) {
                   RR.Response = AST->documentLink(File);
                 });
}

void Server::onStaticCompletion(const lspserver::CompletionParams &Params,
                                lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  auto Action = [Params, this](const nix::ref<EvalAST> &AST,
                               ReplyRAII<CompletionList> &&RR) {
    auto State = IER->Session->getState();
    // Lookup an AST node, and get it's 'Env' after evaluation
    CompletionHelper::Items Items;
    lspserver::log("current trigger character is {0}",
                   Params.context.triggerCharacter);

    if (Params.context.triggerCharacter == ".") {
      RR.Response =
          error("statically lookup attr fields is not yet implemented");
      return;
    }

    const auto *Node = AST->lookupContainMin(Params.position);

    std::vector<nix::Symbol> Symbols;
    AST->collectSymbols(Node, Symbols);
    // Insert symbols to our completion list.
    std::transform(Symbols.begin(), Symbols.end(), std::back_inserter(Items),
                   [&](const nix::Symbol &V) -> decltype(Items)::value_type {
                     decltype(Items)::value_type R;
                     R.kind = CompletionItemKind::Interface;
                     R.label = State->symbols[V];
                     return R;
                   });

    CompletionHelper::fromStaticEnv(State->symbols, *State->staticBaseEnv,
                                    Items);
    // Make the response.
    CompletionList List;
    List.isIncomplete = false;
    List.items = Items;
    RR.Response = List;
  };

  withAST<CompletionList>(Params.textDocument.uri.file().str(),
                          ReplyRAII<CompletionList>(std::move(Reply)),
                          std::move(Action));
}

void Server::onStaticDefinition(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<lspserver::Location> Reply) {
  using namespace lspserver;
  withAST<Location>(
      Params.textDocument.uri.file().str(),
      ReplyRAII<Location>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Location> &&RR) {
        auto State = IER->Session->getState();

        const auto *Node = AST->lookupContainMin(Params.position);
        if (!Node)
          return;

        // try to find the location binds to the variable.
        if (const auto *EVar = dynamic_cast<const nix::ExprVar *>(Node)) {
          try {
            RR.Response = Location{Params.textDocument.uri,
                                   AST->defRange(AST->def(EVar))};
          } catch (std::out_of_range &) {
            RR.Response = error("location not available");
          }
          return;
        }
        RR.Response = error("requested expression is not an ExprVar.");
        return;
      });
}

} // namespace nixd
