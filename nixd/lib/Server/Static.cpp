#include "nixd/Server/Server.h"

namespace nixd {

void Server::switchToStatic() {
  initWorker();
  Role = ServerRole::Static;
  EvalDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");

  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &Server::onStaticCompletion);

  evalInstallable(Config.getEvalDepth());
  mkOutNotifiction<ipc::WorkerMessage>("nixd/ipc/finished")(
      ipc::WorkerMessage{WorkspaceVersion});
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

} // namespace nixd
