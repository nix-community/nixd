#pragma once

#include "Function.h"
#include "Logger.h"
#include "Protocol.h"
#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/JSON.h>

namespace lspserver {

template <typename T>
llvm::Expected<T> parseParam(const llvm::json::Value &Raw,
                             llvm::StringRef PayloadName,
                             llvm::StringRef PayloadKind, T Base = T()) {
  T Result = Base;
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
        llvm::formatv("failed to decode {0} {1}: {2}", PayloadName, PayloadKind,
                      fmt_consume(Root.getError())),
        ErrorCode::InvalidParams);
  }
  return Result;
}
struct HandlerRegistry {
  using JSON = llvm::json::Value;
  template <typename HandlerT>
  using HandlerMap = llvm::StringMap<llvm::unique_function<HandlerT>>;

  HandlerMap<void(JSON)> NotificationHandlers;
  HandlerMap<void(JSON, Callback<JSON>)> MethodHandlers;
  HandlerMap<void(JSON, Callback<JSON>)> CommandHandlers;

public:
  /// Bind a handler for an LSP method.
  /// e.g. method("peek", this, &ThisModule::peek);
  /// Handler should be e.g. void peek(const PeekParams&, Callback<PeekResult>);
  /// PeekParams must be JSON-parseable and PeekResult must be serializable.
  template <typename Param, typename Result, typename ThisT>
  void addMethod(llvm::StringLiteral Method, ThisT *This,
                 void (ThisT::*Handler)(const Param &, Callback<Result>)) {
    MethodHandlers[Method] = [Method, Handler, This](JSON RawParams,
                                                     Callback<JSON> Reply) {
      auto P = parseParam<Param>(RawParams, Method, "request");
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
      llvm::Expected<Param> P = parseParam<Param>(RawParams, Method, "request");
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
      auto P = parseParam<Param>(RawParams, Command, "command");
      if (!P)
        return Reply(P.takeError());
      (This->*Handler)(*P, std::move(Reply));
    };
  }
};

} // namespace lspserver
