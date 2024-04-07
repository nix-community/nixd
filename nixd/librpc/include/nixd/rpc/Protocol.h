#pragma once

#include <bc/Read.h>
#include <bc/Write.h>

#include <llvm/Support/JSON.h>

#include <cstdint>
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
  std::int64_t Size;
};

llvm::json::Value toJSON(const RegisterBCParams &Params);
bool fromJSON(const llvm::json::Value &Params, RegisterBCParams &R,
              llvm::json::Path P);

struct RegisterBCResponse {
  std::optional<std::string> ErrorMsg;
};

inline llvm::json::Value toJSON(const RegisterBCResponse &Params) {
  return llvm::json::Object{{"ErrorMsg", Params.ErrorMsg}};
}
inline bool fromJSON(const llvm::json::Value &Params, RegisterBCResponse &R,
                     llvm::json::Path P) {
  llvm::json::ObjectMapper O(Params, P);
  return O && O.map("ErrorMsg", R.ErrorMsg);
}

struct ExprValueParams {
  std::int64_t ExprID;
};

llvm::json::Value toJSON(const ExprValueParams &Params);
bool fromJSON(const llvm::json::Value &Params, ExprValueParams &R,
              llvm::json::Path P);

struct DerivationDescription {
  /// \brief \p .pname
  std::string PName;

  /// \brief \p .version
  std::string Version;

  /// \brief \p .meta.position
  std::string Position;

  /// \brief \p .meta.description
  std::string Description;
};

llvm::json::Value toJSON(const DerivationDescription &Params);
bool fromJSON(const llvm::json::Value &Params, DerivationDescription &R,
              llvm::json::Path P);

struct ExprValueResponse {
  enum ResultKinds {
    /// \brief The expr is not found in the registered bytecodes.
    NotFound,

    /// \brief The value is available.
    OK,
  };
  int ResultKind;

  std::optional<std::string> ErrorMsg;

  /// \brief A raw message that briefly describes the nix value.
  /// This is supposed to be human-readable.
  std::string Description;

  /// \brief If the value is actually a derivation, we may want to provide more
  /// useful information about it.
  std::optional<DerivationDescription> DrvDesc;
};

llvm::json::Value toJSON(const ExprValueResponse &Params);
bool fromJSON(const llvm::json::Value &Params, ExprValueResponse &R,
              llvm::json::Path P);

} // namespace nixd::rpc
