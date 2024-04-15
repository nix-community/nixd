/// \file
/// \brief Implementation of [Server Lifecycle].
/// [Server Lifecycle]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#lifeCycleMessages

#include "nixd-config.h"

#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Controller.h"

using namespace nixd;
using namespace util;
using namespace llvm::json;
using namespace llvm::cl;
using namespace lspserver;

namespace {

opt<std::string> DefaultNixpkgsExpr{
    "nixpkgs-expr",
    desc("Default expression intrepreted as `import <nixpkgs> { }`"),
    cat(NixdCategory), init("import <nixpkgs> { }")};

opt<std::string> NixpkgsWorkerStderr{
    "nixpkgs-worker-stderr",
    desc("Writable file path for nixpkgs worker stderr (debugging)"),
    cat(NixdCategory), init("/dev/null")};

void notifyNixpkgsEval(AttrSetClient &NixpkgsProvider) {
  lspserver::log("launched nixd attrs eval for nixpkgs");
  auto Action = [](llvm::Expected<EvalExprResponse> Resp) {
    if (!Resp) {
      lspserver::elog("error on nixpkgs attrs worker eval: {0}",
                      Resp.takeError());
      return;
    }
    lspserver::log("evaluated nixpkgs entries");
  };
  // Tell nixpkgs worker to eval
  NixpkgsProvider.evalExpr(DefaultNixpkgsExpr, std::move(Action));
}

void startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval) {
  NixpkgsEval = std::make_unique<AttrSetClientProc>([]() {
    freopen(NixpkgsWorkerStderr.c_str(), "w", stderr);
    return execl(AttrSetClient::getExe(), "nixd-attrset-eval", nullptr);
  });
}

} // namespace

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
       {"documentSymbolProvider", true},
       {
           "semanticTokensProvider",
           Object{
               {
                   "legend",
                   Object{
                       {"tokenTypes",
                        Array{
                            "function",  // function
                            "string",    // string
                            "number",    // number
                            "type",      // select
                            "keyword",   // builtin
                            "variable",  // constant
                            "interface", // fromWith
                            "variable",  // variable
                            "regexp",    // null
                            "macro",     // bool
                            "method",    // attrname
                            "regexp",    // lambdaArg
                            "regexp",    // lambdaFormal
                        }},
                       {"tokenModifiers",
                        Array{
                            "static",   // builtin
                            "abstract", // deprecated
                            "async",    // dynamic
                        }},
                   },
               },
               {"range", false},
               {"full", true},
           },
       },
       {"inlayHintProvider", true},
       {"completionProvider",
        Object{
            {"resolveProvider", true},
        }},
       {"referencesProvider", true},
       {"documentHighlightProvider", true},
       {"hoverProvider", true},
       {"renameProvider",
        Object{
            {"prepareProvider", true},
        }}},
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

  startNixpkgs(NixpkgsEval);
  if (auto *Provider = nixpkgsEval().client())
    notifyNixpkgsEval(*Provider);
}
