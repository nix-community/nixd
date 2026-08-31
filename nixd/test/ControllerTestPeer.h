#pragma once

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

namespace nixd {

struct ControllerTestPeer {
  static void installProviders(Controller &C,
                               ProviderRegistry::WorkerFactory Factory) {
    C.Providers = std::make_unique<ProviderRegistry>(
        C.ConfigStrand, std::move(Factory), std::filesystem::current_path());
  }

  static void apply(Controller &C, Configuration Config,
                    ProviderRegistry::ApplyCallback OnApplied = {}) {
    C.applyConfig(std::move(Config), std::move(OnApplied));
  }

  static bool configHasNixpkgsExpression(Controller &C,
                                         std::string_view Expression) {
    std::lock_guard Guard(C.ConfigLock);
    return C.Config.nixpkgs.expr == Expression;
  }

  static ProviderRegistry &providers(Controller &C) { return *C.Providers; }

  static void prepareFormatting(Controller &C,
                                const std::filesystem::path &File,
                                std::string_view Contents) {
    C.Startup = std::make_unique<const StartupSelection>(StartupSelection{
        .baseConfiguration = defaultConfiguration(),
        .executionCWD = std::filesystem::current_path(),
        .warning = std::nullopt,
    });
    C.Store.addDraft(File.string(), "1", Contents);
  }

  static void
  format(Controller &C, const std::filesystem::path &File,
         lspserver::Callback<std::vector<lspserver::TextEdit>> Reply) {
    lspserver::DocumentFormattingParams Params{
        .textDocument = {.uri = lspserver::URIForFile::canonicalize(
                             File.string(), File.string())},
    };
    C.onFormat(Params, std::move(Reply));
  }

  static std::vector<pid_t> formatterPIDs(Controller &C) {
    return C.Formatters.activePIDs();
  }

  static size_t formattingLaunchAttempts(Controller &C) {
    return C.Formatters.launchAttempts();
  }

  static void setFormattingCommand(Controller &C,
                                   std::vector<std::string> Command) {
    std::lock_guard Guard(C.ConfigLock);
    C.Config.formatting.command = std::move(Command);
  }

  static bool allFormattersCancelled(Controller &C) {
    return C.Formatters.allActiveCancellationRequested();
  }

  static size_t poolSize(Controller &C) {
    return boost::asio::query(C.Pool.get_executor(),
                              boost::asio::execution::occupancy);
  }

  static void shutdown(Controller &C) { C.shutdownController(); }
};

} // namespace nixd
