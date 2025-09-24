#pragma once

#include <llvm/Support/CommandLine.h>

namespace nixd {

extern llvm::cl::OptionCategory NixdCategory;

/// \brief Indicating that we are in lit-test mode.
extern llvm::cl::opt<bool> LitTest;

} // namespace nixd
