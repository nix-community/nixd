#include "nixd-config.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"

#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Controller.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/CommandLine.h>

using namespace lspserver;
using namespace nixd;

namespace {

using namespace llvm::cl;

OptionCategory Misc("miscellaneous options");
OptionCategory Debug("debug-only options (for developers)");

const OptionCategory *NixdCatogories[] = {&Misc, &Debug, &nixd::NixdCategory};

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

} // namespace

int main(int argc, char *argv[]) {
  SetVersionPrinter([](llvm::raw_ostream &OS) {
    OS << "nixd, version: ";
#ifdef NIXD_VCS_TAG
    OS << NIXD_VCS_TAG;
#else
    OS << NIXD_VERSION;
#endif
    OS << "\n";
  });

  HideUnrelatedOptions(NixdCatogories);
  ParseCommandLineOptions(argc, argv, "nixd language server", nullptr,
                          "NIXD_FLAGS");

  if (LitTest) {
    InputStyle = JSONStreamStyle::LitTest;
    LogLevel = Logger::Level::Verbose;
    PrettyPrint = true;
  }

  StreamLogger Logger(llvm::errs(), LogLevel);
  LoggingSession Session(Logger);

  auto In = std::make_unique<lspserver::InboundPort>(STDIN_FILENO, InputStyle);

  auto Out = std::make_unique<lspserver::OutboundPort>(PrettyPrint);

  auto Controller =
      std::make_unique<nixd::Controller>(std::move(In), std::move(Out));

  Controller->run();

  return 0;
}
