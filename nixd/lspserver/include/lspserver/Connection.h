#pragma once

#include <cstdio>

#include <atomic>
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>
#include <mutex>
#include <unistd.h>

namespace lspserver {

enum class JSONStreamStyle {
  // LSP standard, for real lsp server
  Standard,
  // For testing.
  LitTest
};

/// Parsed & classfied messages are dispatched to this handler class
/// LSP Servers should inherit from this handler and dispatch
/// notify/call/reply to implementations.
class MessageHandler {
public:
  virtual ~MessageHandler() = default;
  // Handler returns true to keep processing messages, or false to shut down.
  virtual bool onNotify(llvm::StringRef Method, llvm::json::Value) = 0;
  virtual bool onCall(llvm::StringRef Method, llvm::json::Value Params,
                      llvm::json::Value ID) = 0;
  virtual bool onReply(llvm::json::Value ID,
                       llvm::Expected<llvm::json::Value> Result) = 0;
};

class InboundPort {
private:
  std::atomic<bool> Close;

  template <typename... Ts> void maybeLog(const char *Fmt, Ts &&...Vals);

public:
  int In;

  JSONStreamStyle StreamStyle = JSONStreamStyle::Standard;
  bool LoggingEnabled = true;

  /// Read one message as specified in the LSP standard.
  /// A Language Server Protocol message starts with a set of
  /// HTTP headers, delimited  by \r\n, and terminated by an empty line (\r\n).
  llvm::Expected<llvm::json::Value> readStandardMessage(std::string &Buffer);

  /// Read one message, expecting the input to be one of our Markdown lit-tests.
  llvm::Expected<llvm::json::Value> readLitTestMessage(std::string &Buffer);

  /// \brief Notify the inbound port to close the connection
  void close() { Close = true; }

  void setLoggingEnabled(bool Enabled) { LoggingEnabled = Enabled; }

  InboundPort(int In = STDIN_FILENO,
              JSONStreamStyle StreamStyle = JSONStreamStyle::Standard)
      : Close(false), In(In), StreamStyle(StreamStyle) {};

  /// Read one LSP message depending on the configured StreamStyle.
  /// The parsed value is returned. The given \p Buffer is only used internally.
  llvm::Expected<llvm::json::Value> readMessage(std::string &Buffer);

  /// Dispatch messages to on{Notify,Call,Reply} ( \p Handlers)
  /// Return values should be forwarded from \p Handlers
  /// i.e. returns true to keep processing messages, or false to shut down.
  bool dispatch(llvm::json::Value Message, MessageHandler &Handler);

  void loop(MessageHandler &Handler);
};

class OutboundPort {
private:
  llvm::raw_ostream &Outs;

  llvm::SmallVector<char, 0> OutputBuffer;

  std::mutex Mutex;

  bool Pretty = false;
  bool LoggingEnabled = true;

  template <typename... Ts> void maybeLog(const char *Fmt, Ts &&...Vals);

public:
  explicit OutboundPort(bool Pretty = false)
      : Outs(llvm::outs()), Pretty(Pretty) {}
  OutboundPort(llvm::raw_ostream &Outs, bool Pretty = false)
      : Outs(Outs), OutputBuffer(), Pretty(Pretty) {}
  void notify(llvm::StringRef Method, llvm::json::Value Params);
  void call(llvm::StringRef Method, llvm::json::Value Params,
            llvm::json::Value ID);
  void reply(llvm::json::Value ID, llvm::Expected<llvm::json::Value> Result);

  void sendMessage(llvm::json::Value Message);

  void setLoggingEnabled(bool Enabled) { LoggingEnabled = Enabled; }
};

} // namespace lspserver
