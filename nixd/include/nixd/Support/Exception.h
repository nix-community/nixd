#pragma once

#include <llvm/Support/Error.h>

#include <exception>

namespace nixd {

class LLVMErrorException : public std::exception {
  llvm::Error E;

public:
  LLVMErrorException(llvm::Error E) : E(std::move(E)) {}

  llvm::Error takeError() { return std::move(E); }
};

} // namespace nixd
