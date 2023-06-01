#include "lspserver/Connection.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "nixd/Server.h"

#include <nix/eval.hh>
#include <nix/shared.hh>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/CommandLine.h>

using lspserver::JSONStreamStyle;
using lspserver::Logger;

using namespace llvm::cl;

OptionCategory Misc("miscellaneous options");

const OptionCategory *NixdCatogories[] = {&Misc};

opt<JSONStreamStyle> InputStyle{
    "input-style",
    desc("Input JSON stream encoding"),
    values(
        clEnumValN(JSONStreamStyle::Standard, "standard", "usual LSP protocol"),
        clEnumValN(JSONStreamStyle::Delimited, "delimited",
                   "messages delimited by `// -----` lines, "
                   "with // comment support")),
    init(JSONStreamStyle::Standard),
    cat(Misc),
    Hidden,
};
opt<bool> LitTest{
    "lit-test",
    desc("Abbreviation for -input-style=delimited -pretty -log=verbose. "
         "Intended to simplify lit tests"),
    init(false), cat(Misc)};
opt<Logger::Level> LogLevel{
    "log", desc("Verbosity of log messages written to stderr"),
    values(
        clEnumValN(Logger::Level::Error, "error", "Error messages only"),
        clEnumValN(Logger::Level::Info, "info", "High level execution tracing"),
        clEnumValN(Logger::Level::Debug, "debug", "Debugging details"),
        clEnumValN(Logger::Level::Verbose, "verbose", "Low level details")),
    init(Logger::Level::Info), cat(Misc)};
opt<bool> PrettyPrint{"pretty", desc("Pretty-print JSON output"), init(false),
                      cat(Misc)};

int main(int argc, char *argv[]) {
  using namespace lspserver;
  const char *FlagsEnvVar = "NIXD_FLAGS";
  HideUnrelatedOptions(NixdCatogories);
  ParseCommandLineOptions(argc, argv, "nixd language server", nullptr,
                          FlagsEnvVar);

  if (LitTest) {
    InputStyle = JSONStreamStyle::Delimited;
    LogLevel = Logger::Level::Verbose;
    PrettyPrint = true;
  }

  StreamLogger Logger(llvm::errs(), LogLevel);
  lspserver::LoggingSession Session(Logger);

  lspserver::log("Server started.");
  nixd::Server Server{
      std::make_unique<lspserver::InboundPort>(stdin, InputStyle),
      std::make_unique<lspserver::OutboundPort>(PrettyPrint)};
  Server.run();
  return 0;
}
