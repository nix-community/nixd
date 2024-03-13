#pragma once

#include <bc/Read.h>
#include <bc/Write.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace nixd::rpc {

enum class LogLevel {
  Debug,
  Info,
  Warning,
  Error,
};

enum class RPCKind : uint8_t {
  /// S-method
  RegisterBC,

  /// S-method
  UnregisterBC,

  /// C-notification
  Log,

  /// \brief Query the value of a expr, in registered bytecodes.
  /// S-method
  ExprValue,
};

template <class T> struct Message {
  RPCKind Kind;
  T Params;

  Message() = default;
  Message(RPCKind Kind, T Params) : Kind(Kind), Params(Params) {}
};

template <class T> void writeBytecode(std::ostream &OS, const Message<T> &Msg) {
  using bc::writeBytecode;
  writeBytecode(OS, Msg.Kind);
  writeBytecode(OS, Msg.Params);
}

template <class T> void readBytecode(std::string_view &Data, Message<T> &Msg) {
  using bc::readBytecode;
  readBytecode(Data, Msg.Kind);
  readBytecode(Data, Msg.Params);
}

struct RegisterBCParams {
  std::string Shm;
  std::size_t Size;
  std::uintptr_t BCID;
};

void writeBytecode(std::ostream &OS, const RegisterBCParams &Params);
void readBytecode(std::string_view &Data, RegisterBCParams &Params);

struct ExprValueParams {
  std::uintptr_t BCID;
  std::uintptr_t ExprID;
};

void writeBytecode(std::ostream &OS, const ExprValueParams &Params);
void readBytecode(std::string_view &Data, ExprValueParams &Params);

struct ExprValueResponse {
  enum ResultKinds {
    /// \brief The expr is not found in the registered bytecodes.
    NotFound,

    /// \brief The expr is found, but the value is not evaluated. e.g. too deep
    NotEvaluated,

    /// \brief Encountered an error when evaluating the value.
    EvalError,

    /// \brief The value is available.
    OK,
  } ResultKind;
  /// \brief The value ID, for future reference.
  ///
  /// We may want to query the value of the same expr multiple times, with more
  /// detailed information.
  std::uintptr_t ValueID;

  /// \brief Opaque data, the value of the expr.
  enum ValueKinds {
    Int,
    Float,
  } ValueKind;
};

void writeBytecode(std::ostream &OS, const ExprValueResponse &Params);
void readBytecode(std::string_view &Data, ExprValueResponse &Params);

} // namespace nixd::rpc
