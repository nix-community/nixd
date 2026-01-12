/// \file
/// \brief Implementation of [Formatting].
/// [Formatting]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_formatting
///
/// For now nixd only support "external" formatting. That is, invokes an
/// external command and then let that process do formatting.

#include "nixd/Controller/Controller.h"
#include "nixd/Support/ForkPiped.h"

#include <boost/asio/post.hpp>
#include <sys/wait.h>

using namespace nixd;
using namespace lspserver;

void Controller::onFormat(const DocumentFormattingParams &Params,
                          Callback<std::vector<TextEdit>> Reply) {
  auto Action = [this, Params, Reply = std::move(Reply)]() mutable {
    lspserver::PathRef File = Params.textDocument.uri.file();
    const std::string &Code = *Store.getDraft(File)->Contents;
    // Invokes another process and then read it's stdout.
    std::vector<std::string> FormatCommand;
    {
      // Read from config, get format options.
      std::lock_guard G(ConfigLock);
      FormatCommand = Config.formatting.command;
    }

    if (FormatCommand.empty()) {
      Reply(error("formating command is empty, please set external formatter"));
      return;
    }

    // Convert vectors to syscall form. This should be cheap.
    std::vector<char *> Syscall;
    Syscall.reserve(FormatCommand.size());
    for (const auto &Str : FormatCommand) {
      // For compatibility with existing C code.
      Syscall.emplace_back(const_cast<char *>(Str.c_str()));
    }

    // Null terminator.
    Syscall.emplace_back(nullptr);

    int In;
    int Out;
    int Err;

    pid_t Child = forkPiped(In, Out, Err);
    if (Child == 0) {
      execvp(Syscall[0], Syscall.data());
      exit(-1);
    }
    // Firstly, send the document to the process stdin.
    // Invoke POSIX write(2) to do such thing.
    const char *Start = Code.c_str();
    const char *End = Code.c_str() + Code.size();
    while (Start != End) {
      if (long Writen = write(In, Start, End - Start); Writen != -1) {
        Start += Writen;
      } else {
        throw std::system_error(errno, std::generic_category());
      }
    }
    close(In);

    // And, wait for the process.
    int Exit = 0;
    waitpid(Child, &Exit, 0);

    if (Exit != 0) {
      Reply(error("formatting {0} command exited with {1}", FormatCommand[0],
                  Exit));
      return;
    }

    // Okay, read stdout from it.
    std::string Response;
    while (true) {
      char Buf[1024];
      auto Read = read(Out, Buf, sizeof(Buf));
      if (Read == 0)
        break;
      if (Read < 0)
        throw std::system_error(errno, std::generic_category());
      // Otherwise, append it to "response"
      Response.append(Buf, Read);
    }

    if (Response == Code) {
      Reply(std::vector<TextEdit>{});
      return;
    }

    TextEdit E{{{0, 0}, {INT_MAX, INT_MAX}}, Response};
    Reply(std::vector{E});
  };

  boost::asio::post(Pool, std::move(Action));
}
