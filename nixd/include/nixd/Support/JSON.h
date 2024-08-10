#include "Exception.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/JSON.h>

#include <exception>

namespace nixd {

class JSONParseException : public LLVMErrorException {
public:
  JSONParseException(llvm::Error E) : LLVMErrorException(std::move(E)) {}

  [[nodiscard]] const char *what() const noexcept override {
    return "JSON result cannot be parsed";
  }
};

class JSONSchemaException : public LLVMErrorException {
public:
  JSONSchemaException(llvm::Error E) : LLVMErrorException(std::move(E)) {}

  [[nodiscard]] const char *what() const noexcept override {
    return "JSON schema mismatch";
  }
};

llvm::json::Value parse(llvm::StringRef JSON);

template <class T> T fromJSON(const llvm::json::Value &V) {
  llvm::json::Path::Root R;
  T Result;
  if (!fromJSON(V, Result, R))
    throw JSONSchemaException(R.getError());
  return Result;
}

} // namespace nixd
