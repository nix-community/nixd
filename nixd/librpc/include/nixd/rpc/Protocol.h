#pragma once

#include <bc/Read.h>
#include <bc/Write.h>

#include <llvm/Support/JSON.h>

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

struct RegisterBCParams {
  std::string Shm;
  std::string BasePath;
  std::string CachePath;
  std::size_t Size;
};

llvm::json::Value toJSON(const RegisterBCParams &Params);
bool fromJSON(const llvm::json::Value &Params, RegisterBCParams &R,
              llvm::json::Path P);

struct ExprValueParams {
  std::uintptr_t ExprID;
};

llvm::json::Value toJSON(const ExprValueParams &Params);
bool fromJSON(const llvm::json::Value &Params, ExprValueParams &R,
              llvm::json::Path P);

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
  };
  int ResultKind;
  /// \brief The value ID, for future reference.
  ///
  /// We may want to query the value of the same expr multiple times, with more
  /// detailed information.
  std::uintptr_t ValueID;

  /// \brief Opaque data, the value of the expr.
  enum ValueKinds {
    Int,
    Float,
  };
  int ValueKind;
};

llvm::json::Value toJSON(const ExprValueResponse &Params);
bool fromJSON(const llvm::json::Value &Params, ExprValueResponse &R,
              llvm::json::Path P);

} // namespace nixd::rpc
