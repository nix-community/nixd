#include "nixd/Support/JSON.h"

#include <llvm/ADT/StringRef.h>

llvm::json::Value nixd::parse(llvm::StringRef JSON) {
  llvm::Expected<llvm::json::Value> E = llvm::json::parse(JSON);
  if (!E)
    throw JSONParseException(E.takeError());
  return *E;
}
