#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include <cstdint>
#include <memory>
#include <optional>

namespace lspserver {

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
    // } else {
    //   sendMessage(llvm::json::Object{
    //       {"jsonrpc", "2.0"},
    //       {"id", std::move(ID)},
    //       {"error", encodeError(Result.takeError())},
    //   });
  }
}

void OutboundPort::sendMessage(llvm::json::Value Message) {
  // Make sure our outputs are not interleaving between messages (json)
  vlog(">>> {0}", Message);
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
      // return Handler.onReply(std::move(*ID), decodeError(*Err));
      return false;
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

bool readLine(std::FILE *In, llvm::SmallVectorImpl<char> &Line) {
  static constexpr int BufSize = 128;
  size_t Size = 0;
  Line.clear();
  for (;;) {
    Line.resize_for_overwrite(Size + BufSize);
    std::fgets(&Line[Size], BufSize, In);
    clearerr(In);
    // If the line contained null bytes, anything after it (including \n)
    // will be ignored. Fortunately this is not a legal header or JSON.
    size_t Read = std::strlen(&Line[Size]);
    if (Read > 0 && Line[Size + Read - 1] == '\n') {
      Line.resize(Size + Read);
      return true;
    }
    Size += Read;
  }
}

bool InboundPort::readStandardMessage(std::string &JSONString) {
  unsigned long long ContentLength = 0;
  llvm::SmallString<128> Line;
  while (true) {
    if (feof(In) || ferror(In) || !readLine(In, Line))
      return false;

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

  JSONString.resize(ContentLength);
  for (size_t Pos = 0, Read; Pos < ContentLength; Pos += Read) {
    Read = std::fread(&JSONString[Pos], 1, ContentLength - Pos, In);
    if (Read == 0) {
      elog("Input was aborted. Read only {0} bytes of expected {1}.", Pos,
           ContentLength);
      return false;
    }
    clearerr(In); // If we're done, the error was transient. If we're not done,
                  // either it was transient or we'll see it again on retry.
    Pos += Read;
  }
  return true;
}

bool InboundPort::readDelimitedMessage(std::string &JSONString) {
  JSONString.clear();
  llvm::SmallString<128> Line;
  bool IsInputBlock = false;
  while (readLine(In, Line)) {
    auto LineRef = Line.str().trim();
    if (IsInputBlock) {
      // We are in input blocks, read lines and append JSONString.
      if (LineRef.startswith("#")) // comment
        continue;

      // End of the block
      if (LineRef.startswith("```")) {
        IsInputBlock = false;
        break;
      }

      JSONString += Line;
    } else {
      if (LineRef.startswith("```json"))
        IsInputBlock = true;
    }
  }

  if (ferror(In) != 0) {
    elog("Input error while reading message!");
    return false;
  }
  return true; // Including at EOF
}

bool InboundPort::readMessage(std::string &JSONString) {
  switch (StreamStyle) {

  case JSONStreamStyle::Standard:
    return readStandardMessage(JSONString);
  case JSONStreamStyle::Delimited:
    return readDelimitedMessage(JSONString);
    break;
  }
}

void InboundPort::loop(MessageHandler &Handler) {
  std::string JSONString;
  llvm::SmallString<128> Line;

  while (!feof(stdin)) {
    if (readMessage(JSONString)) {
      vlog("<<< {0}", JSONString);
      if (auto ExpectedParsedJSON = llvm::json::parse(JSONString)) {
        if (!dispatch(*ExpectedParsedJSON, Handler))
          return;
      } else {
        return;
      }
    } else {
      return;
    }
  }
}

} // namespace lspserver
