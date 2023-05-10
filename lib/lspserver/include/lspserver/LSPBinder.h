#pragma once

#include "Function.h"
#include "Logger.h"
#include "Protocol.h"
#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/JSON.h>

namespace lspserver {

struct HandlerRegistry {
  using JSON = llvm::json::Value;
  template <typename HandlerT>
  using HandlerMap = llvm::StringMap<llvm::unique_function<HandlerT>>;

  HandlerMap<void(JSON)> NotificationHandlers;
  HandlerMap<void(JSON, Callback<JSON>)> MethodHandlers;
  HandlerMap<void(JSON, Callback<JSON>)> CommandHandlers;

public:
  template <typename T>
  static llvm::Expected<T> parse(const llvm::json::Value &Raw,
                                 llvm::StringRef PayloadName,
                                 llvm::StringRef PayloadKind) {
    T Result;
    llvm::json::Path::Root Root;
    if (!fromJSON(Raw, Result, Root)) {
      elog("Failed to decode {0} {1}: {2}", PayloadName, PayloadKind,
           Root.getError());
      // Dump the relevant parts of the broken message.
      std::string Context;
      llvm::raw_string_ostream OS(Context);
      Root.printErrorContext(Raw, OS);
      vlog("{0}", OS.str());
      // Report the error (e.g. to the client).
      return llvm::make_error<LSPError>(
          llvm::formatv("failed to decode {0} {1}: {2}", PayloadName,
                        PayloadKind, fmt_consume(Root.getError())),
          ErrorCode::InvalidParams);
    }
    return std::move(Result);
  }

  /// Bind a handler for an LSP method.
  /// e.g. method("peek", this, &ThisModule::peek);
  /// Handler should be e.g. void peek(const PeekParams&, Callback<PeekResult>);
  /// PeekParams must be JSON-parseable and PeekResult must be serializable.
  template <typename Param, typename Result, typename ThisT>
  void addMethod(llvm::StringLiteral Method, ThisT *This,
                 void (ThisT::*Handler)(const Param &, Callback<Result>)) {
    MethodHandlers[Method] = [Method, Handler, This](JSON RawParams,
                                                     Callback<JSON> Reply) {
      auto P = parse<Param>(RawParams, Method, "request");
      if (!P)
        return Reply(P.takeError());
      (This->*Handler)(*P, std::move(Reply));
    };
  }

  /// Bind a handler for an LSP notification.
  /// e.g. notification("poke", this, &ThisModule::poke);
  /// Handler should be e.g. void poke(const PokeParams&);
  /// PokeParams must be JSON-parseable.
  template <typename Param, typename ThisT>
  void addNotification(llvm::StringLiteral Method, ThisT *This,
                       void (ThisT::*Handler)(const Param &)) {
    NotificationHandlers[Method] = [Method, Handler, This](JSON RawParams) {
      llvm::Expected<Param> P = parse<Param>(RawParams, Method, "request");
      if (!P)
        return llvm::consumeError(P.takeError());
      (This->*Handler)(*P);
    };
  }

  /// Bind a handler for an LSP command.
  /// e.g. command("load", this, &ThisModule::load);
  /// Handler should be e.g. void load(const LoadParams&, Callback<LoadResult>);
  /// LoadParams must be JSON-parseable and LoadResult must be serializable.
  template <typename Param, typename Result, typename ThisT>
  void addCommand(llvm::StringLiteral Command, ThisT *This,
                  void (ThisT::*Handler)(const Param &, Callback<Result>)) {
    CommandHandlers[Command] = [Command, Handler, This](JSON RawParams,
                                                        Callback<JSON> Reply) {
      auto P = parse<Param>(RawParams, Command, "command");
      if (!P)
        return Reply(P.takeError());
      (This->*Handler)(*P, std::move(Reply));
    };
  }
};

/// LSPBinder collects a table of functions that handle LSP calls.
///
/// It translates a handler method's signature, e.g.
///    void codeComplete(CompletionParams, Callback<CompletionList>)
/// into a wrapper with a generic signature:
///    void(json::Value, Callback<json::Value>)
/// The wrapper takes care of parsing/serializing responses.
///
/// Incoming calls can be methods, notifications, or commands - all are similar.
///
/// FIXME: this should also take responsibility for wrapping *outgoing* calls,
/// replacing the typed ClangdLSPServer::call<> etc.
class LSPBinder {
public:
  using JSON = llvm::json::Value;
  class RawOutgoing {
  public:
    virtual ~RawOutgoing() = default;
    virtual void callMethod(llvm::StringRef Method, JSON Params,
                            Callback<JSON> Reply) = 0;
    virtual void notify(llvm::StringRef Method, JSON Params) = 0;
  };

  LSPBinder(RawOutgoing &Out) : Out(Out) {}

  template <typename P, typename R>
  using OutgoingMethod = llvm::unique_function<void(const P &, Callback<R>)>;
  /// UntypedOutgoingMethod is convertible to OutgoingMethod<P, R>.
  class UntypedOutgoingMethod;
  /// Bind a function object to be used for outgoing method calls.
  /// e.g. OutgoingMethod<EParams, EResult> Edit = Bind.outgoingMethod("edit");
  /// EParams must be JSON-serializable, EResult must be parseable.
  UntypedOutgoingMethod outgoingMethod(llvm::StringLiteral Method);

  template <typename P>
  using OutgoingNotification = llvm::unique_function<void(const P &)>;
  /// UntypedOutgoingNotification is convertible to OutgoingNotification<T>.
  class UntypedOutgoingNotification;
  /// Bind a function object to be used for outgoing notifications.
  /// e.g. OutgoingNotification<LogParams> Log = Bind.outgoingMethod("log");
  /// LogParams must be JSON-serializable.
  UntypedOutgoingNotification outgoingNotification(llvm::StringLiteral Method);

private:
  RawOutgoing &Out;
};

class LSPBinder::UntypedOutgoingNotification {
  llvm::StringLiteral Method;
  RawOutgoing *Out;
  UntypedOutgoingNotification(llvm::StringLiteral Method, RawOutgoing *Out)
      : Method(Method), Out(Out) {}
  friend UntypedOutgoingNotification
      LSPBinder::outgoingNotification(llvm::StringLiteral);

public:
  template <typename Request> operator OutgoingNotification<Request>() && {
    return
        [Method(Method), Out(Out)](Request R) { Out->notify(Method, JSON(R)); };
  }
};

inline LSPBinder::UntypedOutgoingNotification
LSPBinder::outgoingNotification(llvm::StringLiteral Method) {
  return UntypedOutgoingNotification(Method, &Out);
}

class LSPBinder::UntypedOutgoingMethod {
  llvm::StringLiteral Method;
  RawOutgoing *Out;
  UntypedOutgoingMethod(llvm::StringLiteral Method, RawOutgoing *Out)
      : Method(Method), Out(Out) {}
  friend UntypedOutgoingMethod LSPBinder::outgoingMethod(llvm::StringLiteral);

public:
  template <typename Request, typename Response>
  operator OutgoingMethod<Request, Response>() && {
    return [Method(Method), Out(Out)](Request R, Callback<Response> Reply) {};
  }
};

inline LSPBinder::UntypedOutgoingMethod
LSPBinder::outgoingMethod(llvm::StringLiteral Method) {
  return UntypedOutgoingMethod(Method, &Out);
}

} // namespace lspserver
