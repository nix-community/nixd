/// \file
/// \brief Implementation of [Server Lifecycle].
/// [Server Lifecycle]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#lifeCycleMessages

#include "nixd-config.h"

#include "nixd/CommandLine/Configuration.h"
#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Controller.h"
#include "nixd/Eval/Launch.h"
#include "nixd/Support/Exception.h"

#include "lspserver/Protocol.h"

#include <llvm/Support/CommandLine.h>

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

opt<std::string> DefaultNixOSOptionsExpr{
    "nixos-options-expr",
    desc("Default expression interpreted as option declarations"),
    cat(NixdCategory),
    init("(let pkgs = import <nixpkgs> { }; in (pkgs.lib.evalModules { modules "
         "=  (import <nixpkgs/nixos/modules/module-list.nix>) ++ [ ({...}: { "
         "nixpkgs.hostPlatform = builtins.currentSystem;} ) ] ; })).options")};

opt<std::string> DefaultDevenvOptionsExpr{
    "devenv-options-expr",
    desc("Default expression interpreted as devenv option declarations"),
    cat(NixdCategory),
    init("let\n"
         "  currentSystem = builtins.currentSystem;\n"
         "  flake = builtins.getFlake \"github:cachix/devenv?ref=work/keys/expose-devenv-options\";\n"
         "in\n"
         "  if builtins.hasAttr currentSystem flake.outputs.packages\n"
         "  then flake.outputs.packages.${currentSystem}.devenv-module-options.options\n"
         "  else {}")
};

opt<bool> EnableSemanticTokens{"semantic-tokens",
                               desc("Enable/Disable semantic tokens"),
                               init(true), cat(NixdCategory)};

// Here we try to wrap nixpkgs, nixos options in a single emtpy attrset in test.
std::string getDefaultNixpkgsExpr() {
  if (LitTest && !DefaultNixpkgsExpr.getNumOccurrences()) {
    return "{ }";
  }
  return DefaultNixpkgsExpr;
}

// Check if devenv executable is present
bool isDevenvAvailable() {
    return std::system("which devenv > /dev/null 2>&1") == 0;
}

std::string getDefaultDevenvOptionsExpr() {
  if (LitTest && !DefaultDevenvOptionsExpr.getNumOccurrences()) {
    return "{ }";
  }
  return DefaultDevenvOptionsExpr;
}

std::string getDefaultNixOSOptionsExpr() {
  if (LitTest && !DefaultNixOSOptionsExpr.getNumOccurrences()) {
    return "{ }";
  }
  return DefaultNixOSOptionsExpr;
}

} // namespace

void Controller::evalExprWithProgress(AttrSetClient &Client,
                                      const EvalExprParams &Params,
                                      std::string_view Description) {
  auto Token = rand();
  auto Action = [Token, Description = std::string(Description),
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
       {"documentLinkProvider", Object{}},
       {"documentSymbolProvider", true},
       {"inlayHintProvider", true},
       {"completionProvider",
        Object{
            {"resolveProvider", true},
            {"triggerCharacters", {"."}},
        }},
       {"referencesProvider", true},
       {"documentHighlightProvider", true},
       {"hoverProvider", true},
       {"documentFormattingProvider", true},
       {"renameProvider",
        Object{
            {"prepareProvider", true},
        }}},
  };

  if (EnableSemanticTokens) {
    ServerCaps["semanticTokensProvider"] = Object{
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
    };
  }

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

  // Start default workers.
  startNixpkgs(NixpkgsEval);

  if (nixpkgsClient()) {
    evalExprWithProgress(*nixpkgsClient(), getDefaultNixpkgsExpr(),
                         "nixpkgs entries");
  }

  // Launch nixos worker also.
  {
    std::lock_guard _(OptionsLock);
    startOption("nixos", Options["nixos"]);

    if (AttrSetClient *Client = Options["nixos"]->client())
      evalExprWithProgress(*Client, getDefaultNixOSOptionsExpr(),
                           "nixos options");
  }
  try {
    Config = parseCLIConfig();
  } catch (LLVMErrorException &Err) {
    lspserver::elog("parse CLI config error: {0}, {1}", Err.what(),
                    Err.takeError());
    std::exit(-1);
  }

  // Launch devenv worker only if devenv is available
  if (isDevenvAvailable()) {
      std::lock_guard _(OptionsLock);
      startOption("devenv", Options["devenv"]);

      if (AttrSetClient *Client = Options["devenv"]->client()) {
          evalExprWithProgress(*Client, getDefaultDevenvOptionsExpr(),
                              "devenv options");
      }
  } else {
      lspserver::log("devenv not found, skipping devenv initialization");
  }

  fetchConfig();
}

void Controller::onInitialized(const lspserver::InitializedParams &Params) {}

void Controller::onShutdown(const lspserver::NoParams &,
                            lspserver::Callback<std::nullptr_t> Reply) {
  ReceivedShutdown = true;
  Reply(nullptr);
}
