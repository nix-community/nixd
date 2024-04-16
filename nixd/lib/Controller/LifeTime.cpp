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

#define NULL_DEVICE "/dev/null"

opt<std::string> DefaultNixpkgsExpr{
    "nixpkgs-expr",
    desc("Default expression intrepreted as `import <nixpkgs> { }`"),
    cat(NixdCategory), init("import <nixpkgs> { }")};

opt<std::string> DefaultNixOSOptionsExpr{
    "nixos-options-expr",
    desc("Default expression interpreted as option declarations"),
    cat(NixdCategory),
    init("(let pkgs = import <nixpkgs> { }; in (pkgs.lib.evalModules { modules "
         "=  (import <nixpkgs/nixos/modules/module-list.nix>) ++ [ ({...}: { "
         "nixpkgs.hostPlatform = builtins.currentSystem;} ) ] ; })).options")};

opt<std::string> OptionWorkerStderr{
    "option-worker-stderr", desc("Directory to write options worker stderr"),
    cat(NixdCategory), init(NULL_DEVICE)};

opt<std::string> NixpkgsWorkerStderr{
    "nixpkgs-worker-stderr",
    desc("Writable file path for nixpkgs worker stderr (debugging)"),
    cat(NixdCategory), init(NULL_DEVICE)};

void startAttrSetEval(const std::string &Name,
                      std::unique_ptr<AttrSetClientProc> &Worker) {
  Worker = std::make_unique<AttrSetClientProc>([&Name]() {
    freopen(Name.c_str(), "w", stderr);
    return execl(AttrSetClient::getExe(), "nixd-attrset-eval", nullptr);
  });
}

void startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval) {
  startAttrSetEval(NixpkgsWorkerStderr, NixpkgsEval);
}

void startOption(const std::string &Name,
                 std::unique_ptr<AttrSetClientProc> &Worker) {
  std::string NewName = NULL_DEVICE;
  if (OptionWorkerStderr.getNumOccurrences())
    NewName = OptionWorkerStderr.getValue() + "/" + Name;
  startAttrSetEval(NewName, Worker);
}

} // namespace

void Controller::evalExprWithProgress(AttrSetClient &Client,
                                      const EvalExprParams &Params,
                                      std::string_view Description) {
  auto Token = rand();
  auto Action = [Token, Description,
                 this](llvm::Expected<EvalExprResponse> Resp) {
    endWorkDoneProgress({
        .token = Token,
        .value = WorkDoneProgressEnd{.message = "evaluated " +
                                                std::string(Description)},
    });
    if (!Resp) {
      lspserver::elog("{0} eval expr: {1}", Description, Resp.takeError());
      return;
    }
  };
  createWorkDoneProgress({Token});
  beginWorkDoneProgress({.token = Token,
                         .value = WorkDoneProgressBegin{
                             .title = "evaluating " + std::string(Description),
                             .cancellable = false,
                             .percentage = false,
                         }});
  Client.evalExpr(Params, std::move(Action));
}

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

  ClientCaps = Params.capabilities;

  startNixpkgs(NixpkgsEval);
  if (nixpkgsClient()) {
    evalExprWithProgress(*nixpkgsClient(), DefaultNixpkgsExpr,
                         "nixpkgs entries");
  }

  // Launch nixos worker also.
  startOption("nixos", Options["nixos"]);

  if (AttrSetClient *Client = Options["nixos"]->client())
    evalExprWithProgress(*Client, DefaultNixOSOptionsExpr, "nixos options");
}
