#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/CommandLine.h>

#include <poll.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>

namespace {

llvm::cl::opt<int> ClientProcessID{
    "clientProcessId",
    llvm::cl::desc(
        "Client process ID, if this PID died, the server should exit."),
    llvm::cl::init(getppid())};

std::string jsonToString(llvm::json::Value &Message) {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  OS << Message;
  return Result;
}

class ReadEOF : public llvm::ErrorInfo<ReadEOF> {
public:
  static char ID;
  ReadEOF() = default;

  /// No need to implement this, we don't need to log this error.
  void log(llvm::raw_ostream &OS) const override { __builtin_unreachable(); }

  std::error_code convertToErrorCode() const override {
    return std::make_error_code(std::errc::io_error);
  }
};

char ReadEOF::ID;

} // namespace

namespace lspserver {

template <typename... Ts>
void OutboundPort::maybeLog(const char *Fmt, Ts &&...Vals) {
  if (!LoggingEnabled)
    return;
  vlog(Fmt, std::forward<Ts>(Vals)...);
}

template <typename... Ts>
void InboundPort::maybeLog(const char *Fmt, Ts &&...Vals) {
  if (!LoggingEnabled)
    return;
  vlog(Fmt, std::forward<Ts>(Vals)...);
}

static llvm::json::Object encodeError(llvm::Error Error) {
  std::string Message;
  ErrorCode Code = ErrorCode::UnknownErrorCode;
  auto HandlerFn = [&](const LSPError &E) -> llvm::Error {
    Message = E.Message;
    Code = E.Code;
    return llvm::Error::success();
  };
  if (llvm::Error Unhandled = llvm::handleErrors(std::move(Error), HandlerFn))
    Message = llvm::toString(std::move(Unhandled));

  return llvm::json::Object{
      {"message", std::move(Message)},
      {"code", int64_t(Code)},
  };
}

/// Decode the given JSON object into an error.
llvm::Error decodeError(const llvm::json::Object &O) {
  llvm::StringRef Message =
      O.getString("message").value_or("Unspecified error");
  if (std::optional<int64_t> Code = O.getInteger("code"))
    return llvm::make_error<LSPError>(Message.str(), ErrorCode(*Code));
  return llvm::make_error<llvm::StringError>(llvm::inconvertibleErrorCode(),
                                             Message.str());
}

void OutboundPort::notify(llvm::StringRef Method, llvm::json::Value Params) {
  sendMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", Method},
      {"params", std::move(Params)},
  });
}
void OutboundPort::call(llvm::StringRef Method, llvm::json::Value Params,
                        llvm::json::Value ID) {
  sendMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", std::move(ID)},
      {"method", Method},
      {"params", std::move(Params)},
  });
}
void OutboundPort::reply(llvm::json::Value ID,
                         llvm::Expected<llvm::json::Value> Result) {
  if (Result) {
    sendMessage(llvm::json::Object{
        {"jsonrpc", "2.0"},
        {"id", std::move(ID)},
        {"result", std::move(*Result)},
    });
  } else {
    sendMessage(llvm::json::Object{
        {"jsonrpc", "2.0"},
        {"id", std::move(ID)},
        {"error", encodeError(Result.takeError())},
    });
  }
}

void OutboundPort::sendMessage(llvm::json::Value Message) {
  // Make sure our outputs are not interleaving between messages (json)
  maybeLog(">>> {0}", Message);
  std::lock_guard<std::mutex> Guard(Mutex);
  OutputBuffer.clear();
  llvm::raw_svector_ostream SVecOS(OutputBuffer);
  SVecOS << (Pretty ? llvm::formatv("{0:2}", Message)
                    : llvm::formatv("{0}", Message));
  Outs << "Content-Length: " << OutputBuffer.size() << "\r\n\r\n"
       << OutputBuffer;
  Outs.flush();
}

bool InboundPort::dispatch(llvm::json::Value Message, MessageHandler &Handler) {
  // Message must be an object with "jsonrpc":"2.0".
  auto *Object = Message.getAsObject();
  if (!Object ||
      Object->getString("jsonrpc") != std::optional<llvm::StringRef>("2.0")) {
    elog("Not a JSON-RPC 2.0 message: {0:2}", Message);
    return false;
  }
  // ID may be any JSON value. If absent, this is a notification.
  std::optional<llvm::json::Value> ID;
  if (auto *I = Object->get("id"))
    ID = std::move(*I);
  auto Method = Object->getString("method");
  if (!Method) { // This is a response.
    if (!ID) {
      elog("No method and no response ID: {0:2}", Message);
      return false;
    }
    if (auto *Err = Object->getObject("error"))
      // TODO: Logging & reply errors.
      return Handler.onReply(std::move(*ID), decodeError(*Err));
    // Result should be given, use null if not.
    llvm::json::Value Result = nullptr;
    if (auto *R = Object->get("result"))
      Result = std::move(*R);
    return Handler.onReply(std::move(*ID), std::move(Result));
  }
  // Params should be given, use null if not.
  llvm::json::Value Params = nullptr;
  if (auto *P = Object->get("params"))
    Params = std::move(*P);

  if (ID)
    return Handler.onCall(*Method, std::move(Params), std::move(*ID));
  return Handler.onNotify(*Method, std::move(Params));
}

bool readLine(int fd, const std::atomic<bool> &Close,
              llvm::SmallString<128> &Line) {
  Line.clear();

  std::vector<pollfd> FDs;
  FDs.emplace_back(pollfd{
      fd,
      POLLIN | POLLPRI,
      0,
  });
  for (;;) {
    char Ch;
    // FIXME: inefficient
    int Poll = poll(FDs.data(), FDs.size(), 1000);
    if (Poll < 0)
      return false;
    if (Close)
      return false;

    // POLLHUB: hang up from the writer side
    // POLLNVAL: invalid request (fd not open)
    if (FDs[0].revents & (POLLHUP | POLLNVAL)) {
      return false;
    }

    if (FDs[0].revents & POLLIN) {
      ssize_t BytesRead = read(fd, &Ch, 1);
      if (BytesRead == -1) {
        if (errno != EINTR)
          return false;
      } else if (BytesRead == 0)
        return false;
      else {
        if (Ch == '\n')
          return true;
        Line += Ch;
      }
    }

    if (kill(ClientProcessID, 0) < 0) {
      // Parent died.
      return false;
    }
  }
}

llvm::Expected<llvm::json::Value>
InboundPort::readStandardMessage(std::string &Buffer) {
  unsigned long long ContentLength = 0;
  llvm::SmallString<128> Line;
  while (true) {
    if (!readLine(In, Close, Line))
      return llvm::make_error<ReadEOF>(); // EOF

    llvm::StringRef LineRef = Line;

    // Content-Length is a mandatory header, and the only one we handle.
    if (LineRef.consume_front("Content-Length: ")) {
      llvm::getAsUnsignedInteger(LineRef.trim(), 0, ContentLength);
      continue;
    }
    // An empty line indicates the end of headers.
    // Go ahead and read the JSON.
    if (LineRef.trim().empty())
      break;
    // It's another header, ignore it.
  }

  Buffer.resize(ContentLength);
  for (size_t Pos = 0, Read; Pos < ContentLength; Pos += Read) {

    Read = read(In, Buffer.data() + Pos, ContentLength - Pos);

    if (Read == 0) {
      elog("Input was aborted. Read only {0} bytes of expected {1}.", Pos,
           ContentLength);
      return llvm::make_error<ReadEOF>();
    }
  }
  return llvm::json::parse(Buffer);
}

llvm::Expected<llvm::json::Value>
InboundPort::readLitTestMessage(std::string &Buffer) {
  enum class State { Prose, JSONBlock, NixBlock };
  State State = State::Prose;
  Buffer.clear();
  llvm::SmallString<128> Line;
  std::string NixDocURI;
  while (readLine(In, Close, Line)) {
    auto LineRef = Line.str().trim();
    if (State == State::Prose) {
      if (LineRef.starts_with("```json"))
        State = State::JSONBlock;
      else if (LineRef.consume_front("```nix ")) {
        State = State::NixBlock;
        NixDocURI = LineRef.str();
      }
    } else if (State == State::JSONBlock) {
      // We are in a JSON block, read lines and append JSONString.
      if (LineRef.starts_with("#")) // comment
        continue;

      // End of the block
      if (LineRef.starts_with("```")) {
        return llvm::json::parse(Buffer);
      }

      Buffer += Line;
    } else if (State == State::NixBlock) {
      // We are in a Nix block. (This was implemented to make the .md test
      // files more readable, particularly regarding multiline Nix documents,
      // so that the newlines don't have to be \n escaped.)

      if (LineRef.starts_with("```")) {
        return llvm::json::Object{
            {"jsonrpc", "2.0"},
            {"method", "textDocument/didOpen"},
            {
                "params",
                llvm::json::Object{
                    {
                        "textDocument",
                        llvm::json::Object{
                            {"uri", NixDocURI},
                            {"languageId", "nix"},
                            {"version", 1},
                            {"text", llvm::StringRef(Buffer).rtrim().str()},
                        },
                    },
                },
            }};
      }
      Buffer += Line;
      Buffer += "\n";
    } else {
      assert(false && "unreachable");
    }
  }
  return llvm::make_error<ReadEOF>(); // EOF
}

llvm::Expected<llvm::json::Value>
InboundPort::readMessage(std::string &Buffer) {
  switch (StreamStyle) {

  case JSONStreamStyle::Standard:
    return readStandardMessage(Buffer);
  case JSONStreamStyle::LitTest:
    return readLitTestMessage(Buffer);
  }
  assert(false && "Invalid stream style");
  __builtin_unreachable();
}

void InboundPort::loop(MessageHandler &Handler) {
  std::string Buffer;

  for (;;) {
    if (auto Message = readMessage(Buffer)) {
      maybeLog("<<< {0}", jsonToString(*Message));
      if (!dispatch(*Message, Handler))
        return;
    } else {
      // Handle error while reading message.
      return [&]() {
        auto Err = Message.takeError();
        if (Err.isA<ReadEOF>())
          return; // Stop reading.
        else if (Err.isA<llvm::json::ParseError>())
          elog("The received json cannot be parsed, reason: {0}", Err);
        else
          elog("Error reading message: {0}", llvm::toString(std::move(Err)));
      }();
    }
  }
}

} // namespace lspserver
