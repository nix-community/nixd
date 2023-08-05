#pragma once

#include "Config.h"

#include <llvm/Support/JSON.h>

namespace nixd::configuration {

llvm::json::Value toJSON(const TopLevel::Eval &R);

llvm::json::Value toJSON(const TopLevel::Formatting &R);

llvm::json::Value toJSON(const TopLevel::Options &R);

llvm::json::Value toJSON(const InstallableConfigurationItem &R);

bool fromJSON(const llvm::json::Value &Params, TopLevel::Eval &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, TopLevel::Formatting &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, TopLevel::Options &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, TopLevel &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, InstallableConfigurationItem &R,
              llvm::json::Path P);

} // namespace nixd::configuration
