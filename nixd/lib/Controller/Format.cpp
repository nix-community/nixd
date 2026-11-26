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

#include <cerrno>
#include <csignal>
#include <thread>

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
      _exit(-1);
    }

    // Read the formatter's stdout on another thread while this one writes
    // the document to its stdin. Writing everything, waiting for the
    // process and only then reading deadlocks as soon as the formatted
    // output fills the pipe (64 KiB on Linux): the formatter blocks in
    // write(2) and nixd blocks in waitpid(2).
    std::string Response;
    std::thread Reader([&Response, Out]() {
      char Buf[4096];
      for (;;) {
        ssize_t Read = read(Out, Buf, sizeof(Buf));
        if (Read < 0 && errno == EINTR)
          continue;
        if (Read <= 0)
          break;
        Response.append(Buf, Read);
      }
    });

    // Send the document to the formatter's stdin. A formatter may exit
    // without reading all of it, and a write to a pipe nobody reads raises
    // SIGPIPE, which would kill nixd. Block SIGPIPE in this thread so the
    // write fails with EPIPE instead, then drop the pending signal.
    sigset_t PipeSignal;
    sigset_t OldMask;
    sigemptyset(&PipeSignal);
    sigaddset(&PipeSignal, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &PipeSignal, &OldMask);
    const char *Start = Code.c_str();
    const char *End = Code.c_str() + Code.size();
    while (Start != End) {
      ssize_t Written = write(In, Start, End - Start);
      if (Written >= 0) {
        Start += Written;
        continue;
      }
      if (errno == EINTR)
        continue;
      // EPIPE: the formatter stopped reading. Its exit status decides.
      break;
    }
    close(In);
    // sigtimedwait(2) would do this in one call, but macOS lacks it. Only
    // call sigwait(3) when SIGPIPE is pending so it never blocks.
    sigset_t Pending;
    sigpending(&Pending);
    if (sigismember(&Pending, SIGPIPE)) {
      int Signal;
      sigwait(&PipeSignal, &Signal);
    }
    pthread_sigmask(SIG_SETMASK, &OldMask, nullptr);

    // Wait for the output to end and for the process to exit.
    Reader.join();
    close(Out);
    close(Err);
    int Exit = 0;
    waitpid(Child, &Exit, 0);

    if (Exit != 0) {
      Reply(error("formatting {0} command exited with {1}", FormatCommand[0],
                  Exit));
      return;
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
