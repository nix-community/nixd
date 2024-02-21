#pragma once

#include "Protocol.h"

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
void readBytecode(std::string_view &Data, const ExprValueParams &Params);

struct ExprValueResponse {
  enum class Kind {
    /// \brief The expr is not found in the registered bytecodes.
    NotFound,

    /// \brief The expr is found, but the value is not evaluated. e.g. too deep
    NotEvaluated,

    /// \brief Encountered an error when evaluating the value.
    EvalError,

    /// \brief The value is available.
    OK,
  };
  /// \brief The value ID, for future reference.
  ///
  /// We may want to query the value of the same expr multiple times, with more
  /// detailed information.
  std::uintptr_t ValueID;

  /// \brief Opaque data, the value of the expr.
  enum class ValueKind {
    Int,
    Float,
  };
};

} // namespace nixd::rpc
