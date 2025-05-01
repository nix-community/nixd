#include "nixd-config.h"

#include "nixd/CommandLine/Options.h"
#include "nixd/Eval/AttrSetProvider.h"

#include <llvm/Support/CommandLine.h>
#include <lspserver/Connection.h>
#include <nixt/InitEval.h>

#include <unistd.h>

using namespace llvm::cl;
using namespace lspserver;
using namespace nixd;

namespace {

OptionCategory Misc("miscellaneous options");
OptionCategory Debug("debug-only options (for developers)");

opt<JSONStreamStyle> InputStyle{
    "input-style",
    desc("Input JSON stream encoding"),
    values(
        clEnumValN(JSONStreamStyle::Standard, "standard", "usual LSP protocol"),
        clEnumValN(JSONStreamStyle::LitTest, "lit-test",
                   "Input format for lit-testing")),
    init(JSONStreamStyle::Standard),
    cat(Debug),
    Hidden,
};

opt<Logger::Level> LogLevel{
    "log", desc("Verbosity of log messages written to stderr"),
    values(
        clEnumValN(Logger::Level::Error, "error", "Error messages only"),
        clEnumValN(Logger::Level::Info, "info", "High level execution tracing"),
        clEnumValN(Logger::Level::Debug, "debug", "Debugging details"),
        clEnumValN(Logger::Level::Verbose, "verbose", "Low level details")),
    init(Logger::Level::Info), cat(Misc)};

opt<bool> PrettyPrint{"pretty", desc("Pretty-print JSON output"), init(false),
                      cat(Debug)};

const OptionCategory *Catogories[] = {&Misc, &Debug};

} // namespace

int main(int Argc, const char *Argv[]) {

  SetVersionPrinter([](llvm::raw_ostream &OS) {
    OS << "nixd-attrset-eval, version: ";
#ifdef NIXD_VCS_TAG
    OS << NIXD_VCS_TAG;
#else
    OS << NIXD_VERSION;
#endif
    OS << "\n";
  });

  HideUnrelatedOptions(Catogories);
  ParseCommandLineOptions(Argc, Argv, "nixd nixpkgs evaluator", nullptr,
                          "NIXD_NIXPKGS_EVAL_FLAGS");

  if (LitTest) {
    InputStyle = JSONStreamStyle::LitTest;
    LogLevel = Logger::Level::Verbose;
    PrettyPrint = true;
  }

  StreamLogger Logger(llvm::errs(), LogLevel);
  LoggingSession Session(Logger);

  nixt::initEval();
  auto In = std::make_unique<lspserver::InboundPort>(STDIN_FILENO, InputStyle);

  auto Out = std::make_unique<lspserver::OutboundPort>(PrettyPrint);
  nixd::AttrSetProvider Provider(std::move(In), std::move(Out));

  Provider.run();
}
