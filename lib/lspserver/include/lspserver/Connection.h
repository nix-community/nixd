#pragma once

#include <cstdio>

#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>
#include <mutex>

namespace lspserver {

/// Parsed & classfied messages are dispatched to this handler class
/// LSP Servers should inherit from this hanlder and dispatch
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
  std::FILE *In;

public:
  InboundPort(std::FILE *In = stdin) : In(In){};

  /// Read messages specified in LSP standard, and collect standard json string
  /// into \p JSONString.
  /// A Language Server Protocol message starts with a set of
  /// HTTP headers, delimited  by \r\n, and terminated by an empty line (\r\n).
  bool readMessage(std::string &JSONString);

  /// Dispatch messages to on{Notify,Call,Reply} ( \p Handlers)
  /// Return values should be forwarded from \p Handlers
  /// i.e. returns true to keep processing messages, or false to shut down.
  bool dispatch(llvm::json::Value Message, MessageHandler &Hanlder);

  void loop(MessageHandler &Handler);
};

class OutboundPort {
private:
  llvm::raw_ostream &Outs;

  llvm::SmallVector<char, 0> OutputBuffer;

  std::mutex Mutex;

public:
  OutboundPort() : Outs(llvm::outs()) {}
  OutboundPort(llvm::raw_ostream &Outs) : Outs(Outs), OutputBuffer() {}
  void notify(llvm::StringRef Method, llvm::json::Value Params);
  void call(llvm::StringRef Method, llvm::json::Value Params,
            llvm::json::Value ID);
  void reply(llvm::json::Value ID, llvm::Expected<llvm::json::Value> Result);

  void sendMessage(llvm::json::Value Message);
};

} // namespace lspserver
