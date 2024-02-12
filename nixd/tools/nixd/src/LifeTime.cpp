/// \file
/// \brief Implementation of [Server Lifecycle].
/// [Server Lifecycle]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#lifeCycleMessages

#include "nixd-config.h"

#include "Controller.h"

namespace nixd {

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
}

} // namespace nixd
