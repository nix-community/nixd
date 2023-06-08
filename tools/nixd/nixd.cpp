#include "nixd-config.h"

#include "lspserver/Connection.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"

#include "nixd/Server.h"

#include <nix/eval.hh>
#include <nix/shared.hh>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/CommandLine.h>

// Fix build on macOS. See
// https://github.com/nix-community/nixd/actions/runs/5208659631/jobs/9397498730?pr=90
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>

#include <filesystem>
#include <fstream>

#include <csignal>
#include <unistd.h>

using lspserver::JSONStreamStyle;
using lspserver::Logger;

namespace nixd {

constexpr const char *StackDumpFile = "nixd-backtrace.dump";

void printReportInfo() {
  std::cerr
      << "Please file an issue at https://github.com/nix-community/nixd/issues/"
      << "\n";
}

void sigHandler(int Signum) {
  ::signal(Signum, SIG_DFL);
  boost::stacktrace::safe_dump_to(StackDumpFile);

  // POSIX say we can use `read` and `write` syscall, this is async-signal-safe
  printReportInfo();
  std::cerr << boost::stacktrace::stacktrace() << "\n";
  ::raise(SIGABRT);
}

void checkStackDump() {
  if (std::filesystem::exists(StackDumpFile)) {
    // there is a backtrace
    std::ifstream IFS(StackDumpFile);

    boost::stacktrace::stacktrace ST =
        boost::stacktrace::stacktrace::from_dump(IFS);

    printReportInfo();
    std::cerr << "Previous run crashed:\n" << ST << std::endl;

    // cleaning up
    IFS.close();
    std::filesystem::remove(StackDumpFile);
  }
}

void registerSigHanlder() {
  signal(SIGSEGV, sigHandler);
  signal(SIGABRT, sigHandler);
}

} // namespace nixd

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

opt<int> WaitWorker{"wait-worker",
                    desc("Microseconds to wait before exit (for testing)"),
                    init(0), cat(Misc), Hidden};

int main(int argc, char *argv[]) {
  using namespace lspserver;
  nixd::registerSigHanlder();
  nixd::checkStackDump();
  const char *FlagsEnvVar = "NIXD_FLAGS";
  HideUnrelatedOptions(NixdCatogories);
  ParseCommandLineOptions(argc, argv, "nixd language server", nullptr,
                          FlagsEnvVar);

  if (LitTest) {
    InputStyle = JSONStreamStyle::Delimited;
    LogLevel = Logger::Level::Verbose;
    PrettyPrint = true;
    if (!WaitWorker)
      WaitWorker = 2e5; // 0.2s
  }

  StreamLogger Logger(llvm::errs(), LogLevel);
  lspserver::LoggingSession Session(Logger);

  lspserver::log("nixd {0} started", NIXD_VERSION);
  nixd::Server Server{
      std::make_unique<lspserver::InboundPort>(STDIN_FILENO, InputStyle),
      std::make_unique<lspserver::OutboundPort>(PrettyPrint), WaitWorker};
  Server.run();
  return 0;
}
