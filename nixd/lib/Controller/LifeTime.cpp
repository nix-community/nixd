/// \file
/// \brief Implementation of [Server Lifecycle].
/// [Server Lifecycle]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#lifeCycleMessages

#include "nixd-config.h"

#include "nixd/Controller/Controller.h"
#include "nixd/Controller/EvalClient.h"
#include "nixd/Support/PipedProc.h"

#include <cstring>

namespace nixd {

using namespace util;
using namespace llvm::json;
using namespace lspserver;

void Controller::
    onInitialize( // NOLINT(readability-convert-member-functions-to-static)
        [[maybe_unused]] const InitializeParams &Params,
        Callback<Value> Reply) {

  Object ServerCaps{
      {{"textDocumentSync",
        llvm::json::Object{
            {"openClose", true},
            {"change", (int)TextDocumentSyncKind::Incremental},
            {"save", true},
        }},
       {
           "codeActionProvider",
           Object{
               {"codeActionKinds", Array{CodeAction::QUICKFIX_KIND}},
               {"resolveProvider", false},
           },
       },
       {"definitionProvider", true},
       {"hoverProvider", true}},
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

  PublishDiagnostic = mkOutNotifiction<PublishDiagnosticsParams>(
      "textDocument/publishDiagnostics");

  int Fail;
  Eval = OwnedEvalClient::create(Fail);
  if (Fail != 0) {
    lspserver::elog("failed to create nix-node-eval worker: {0}",
                    strerror(-Fail));
  } else {
    lspserver::log("launched nix-node-eval instance: {0}", Eval->proc().PID);
  }
}

} // namespace nixd
