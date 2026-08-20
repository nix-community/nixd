/// \file
/// \brief Implementation of [Formatting].
/// [Formatting]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_formatting
///
/// For now nixd only support "external" formatting. That is, invokes an
/// external command and then let that process do formatting.

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>
#include <exception>

using namespace nixd;
using namespace lspserver;

void Controller::onFormat(const DocumentFormattingParams &Params,
                          Callback<std::vector<TextEdit>> Reply) {
  assert(Startup);
  const std::filesystem::path ExecutionCWD = Startup->executionCWD;
  auto Action = [this, Params, ExecutionCWD,
                 Reply = std::move(Reply)]() mutable {
    try {
      lspserver::PathRef File = Params.textDocument.uri.file();
      auto Draft = Store.getDraft(File);
      if (!Draft) {
        Reply(error("cannot format an unopened document"));
        return;
      }
      const std::string Code = *Draft->Contents;
      std::vector<std::string> FormatCommand;
      {
        std::lock_guard G(ConfigLock);
        FormatCommand = Config.formatting.command;
      }

      if (FormatCommand.empty()) {
        Reply(
            error("formating command is empty, please set external formatter"));
        return;
      }

      FormatterRunResult Result =
          runFormatter(Formatters, FormatCommand, ExecutionCWD, Code);
      if (Result.Cancelled) {
        Reply(error("formatting cancelled because nixd is shutting down"));
        return;
      }
      if (Result.ExitStatus != 0) {
        Reply(error("formatting {0} command exited with {1}", FormatCommand[0],
                    Result.ExitStatus));
        return;
      }

      if (Result.Stdout == Code) {
        Reply(std::vector<TextEdit>{});
        return;
      }

      TextEdit E{{{0, 0}, {INT_MAX, INT_MAX}}, std::move(Result.Stdout)};
      Reply(std::vector{std::move(E)});
    } catch (const std::exception &Err) {
      Reply(error("formatting command failed: {0}", Err.what()));
    } catch (...) {
      Reply(error("formatting command failed with an unknown exception"));
    }
  };

  boost::asio::post(Pool, std::move(Action));
}
