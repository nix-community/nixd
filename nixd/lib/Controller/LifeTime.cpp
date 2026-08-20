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

#include <chrono>
#include <filesystem>
#include <semaphore>

using namespace nixd;
using namespace util;
using namespace llvm::json;
using namespace llvm::cl;
using namespace lspserver;

namespace {

opt<std::string> DefaultNixpkgsExpr{
    "nixpkgs-expr",
    desc("Default expression intrepreted as `import <nixpkgs> { }`"),
    cat(NixdCategory), init("")};

opt<std::string> DefaultNixOSOptionsExpr{
    "nixos-options-expr",
    desc("Default expression interpreted as option declarations"),
    cat(NixdCategory), init("")};

opt<bool> EnableSemanticTokens{"semantic-tokens",
                               desc("Enable/Disable semantic tokens"),
                               init(false), cat(NixdCategory)};

ConfigurationPatch legacyCLIConfig() {
  ConfigurationPatch Patch;
  if (DefaultNixpkgsExpr.getNumOccurrences())
    Patch.nixpkgs = {.expr = DefaultNixpkgsExpr};
  if (DefaultNixOSOptionsExpr.getNumOccurrences())
    Patch.options = {{"nixos", {.expr = DefaultNixOSOptionsExpr}}};
  return Patch;
}

ConfigurationPatch litTestDefaults() {
  ConfigurationPatch Patch;
  if (!LitTest)
    return Patch;
  if (!DefaultNixpkgsExpr.getNumOccurrences())
    Patch.nixpkgs = {.expr = "{ }"};
  if (!DefaultNixOSOptionsExpr.getNumOccurrences())
    Patch.options = {{"nixos", {.expr = "{ }"}}};
  return Patch;
}

class AttrSetProviderWorker final : public ProviderWorker {
  std::unique_ptr<AttrSetClientProc> Process;

public:
  AttrSetProviderWorker(const ProviderKey &Key,
                        const std::filesystem::path &CWD,
                        DeathCallback OnDeath) {
    if (Key.Kind == ProviderKind::Nixpkgs)
      startNixpkgs(Process, CWD, std::move(OnDeath));
    else
      startOption(Key.Name, Process, CWD, std::move(OnDeath));
  }

  void evaluate(std::string Expression, EvaluationCallback Reply) override {
    auto *Client = attrSetClient();
    if (!Client) {
      Reply(false);
      return;
    }
    Client->evalExpr(Expression,
                     [Reply = std::move(Reply)](
                         llvm::Expected<EvalExprResponse> Response) mutable {
                       const bool Success = static_cast<bool>(Response);
                       if (!Response)
                         lspserver::elog("provider evaluation failed: {0}",
                                         Response.takeError());
                       Reply(Success);
                     });
  }

  void cancel() override {
    if (Process)
      Process->stop();
  }

  [[nodiscard]] bool alive() const override {
    return Process && Process->alive();
  }

  [[nodiscard]] AttrSetClient *attrSetClient() override {
    return Process ? Process->client() : nullptr;
  }
};

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
               {"codeActionKinds",
                Array{CodeAction::QUICKFIX_KIND, CodeAction::REFACTOR_KIND,
                      CodeAction::REFACTOR_REWRITE_KIND}},
               {"resolveProvider", true},
           },
       },
       {"definitionProvider", true},
       {"documentLinkProvider", Object{}},
       {"documentSymbolProvider", true},
       {"foldingRangeProvider", true},
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

  ClientCaps = Params.capabilities;

  CommandLineConfiguration CLI;
  try {
    CLI = parseCLIConfig(litTestDefaults(), legacyCLIConfig());
  } catch (LLVMErrorException &Err) {
    lspserver::elog("parse CLI config error: {0}, {1}", Err.what(),
                    Err.takeError());
    std::exit(-1);
  }

  Startup = std::make_unique<const StartupSelection>(
      selectStartup(CLI, Params, std::filesystem::current_path()));
  if (Startup->warning)
    lspserver::elog("{0}", *Startup->warning);

  Providers = std::make_unique<ProviderRegistry>(
      ConfigStrand,
      [](const ProviderKey &Key, const std::filesystem::path &CWD,
         ProviderWorker::DeathCallback OnDeath) {
        return std::make_shared<AttrSetProviderWorker>(Key, CWD,
                                                       std::move(OnDeath));
      },
      Startup->executionCWD);
  EditorConfig.emplace(
      ConfigStrand, Startup->baseConfiguration,
      [this](Configuration Config) { applyConfig(std::move(Config)); },
      [](std::string Error) { lspserver::elog("{0}", Error); });
  // Startup queries historically become usable as soon as initialize
  // returns. Keep Active-only query tokens by waiting here, on the LSP input
  // thread, while ConfigStrand and worker input threads continue to run. A
  // provider error settles as Failed. A hung evaluator is bounded: initialize
  // proceeds after the timeout and that provider remains unavailable until it
  // later settles.
  auto Configured = std::make_shared<std::binary_semaphore>(0);
  updateConfig(Startup->baseConfiguration,
               [Configured](ProviderApplyResult) { Configured->release(); });
  if (!Configured->try_acquire_for(std::chrono::seconds(5)))
    lspserver::elog("initial provider evaluation did not settle within 5s");

  Reply(std::move(Result));
  // workspace/configuration is requested only after initialize has replied;
  // some clients do not service server requests before that point.
  fetchConfig();
}

void Controller::onInitialized(const lspserver::InitializedParams &Params) {
  if (!Startup || !Startup->warning)
    return;
  std::call_once(StartupWarningOnce, [this] {
    ShowMessage({
        .type = MessageType::Warning,
        .message = *Startup->warning,
    });
  });
}

void Controller::onShutdown(const lspserver::NoParams &,
                            lspserver::Callback<std::nullptr_t> Reply) {
  shutdownController();
  Reply(nullptr);
}

void Controller::shutdownController() {
  {
    std::unique_lock Lock(ShutdownMutex);
    if (ShutdownState == ShutdownPhase::Stopped)
      return;
    if (ShutdownState == ShutdownPhase::Stopping) {
      ShutdownChanged.wait(
          Lock, [this] { return ShutdownState == ShutdownPhase::Stopped; });
      return;
    }
    ShutdownState = ShutdownPhase::Stopping;
  }

  Accepting = false;
  closeRequestGate("nixd is shutting down");
  Formatters.cancelAll();
  if (EditorConfig)
    EditorConfig->stop();

  if (Providers) {
    std::binary_semaphore Retired(0);
    Providers->shutdown([&Retired] { Retired.release(); });
    Retired.acquire();
  }

  // shutdownController is called only by the LSP input/owner thread, never by
  // a Pool task. The provider-registry waiter above runs on ConfigStrand and
  // releases the semaphore; joining here drains query tokens before output
  // state can be destroyed.
  Pool.join();

  {
    std::lock_guard Lock(ShutdownMutex);
    ShutdownState = ShutdownPhase::Stopped;
  }
  ShutdownChanged.notify_all();
}

Controller::~Controller() { shutdownController(); }
