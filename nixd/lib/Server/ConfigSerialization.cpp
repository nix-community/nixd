#include "nixd/Server/ConfigSerialization.h"

#include <llvm/Support/JSON.h>

namespace nixd::configuration {

using llvm::json::ObjectMapper;
using llvm::json::Path;
using llvm::json::Value;

bool fromJSON(const Value &Params, TopLevel::Eval &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("depth", R.depth) &&
         O.mapOptional("target", R.target) &&
         O.mapOptional("workers", R.workers);
}

bool fromJSON(const Value &Params, TopLevel::Formatting &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("command", R.command);
}

bool fromJSON(const Value &Params, TopLevel::Options &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("enable", R.enable) &&
         O.mapOptional("target", R.target);
}

bool fromJSON(const Value &Params, TopLevel &R, Path P) {
  Value X = Params;
  if (Params.kind() == Value::Array) {
    const auto *PA = Params.getAsArray();
    X = PA->front();
  }
  ObjectMapper O(X, P);

  return O && O.mapOptional("eval", R.eval) &&
         O.mapOptional("formatting", R.formatting) &&
         O.mapOptional("options", R.options);
}

bool fromJSON(const Value &Params, std::list<std::string> &R, Path P) {
  std::vector<std::string> RVec{std::begin(R), std::end(R)};
  return fromJSON(Params, RVec, P);
}

bool fromJSON(const Value &Params, InstallableConfigurationItem &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("args", R.args) &&
         O.mapOptional("installable", R.installable);
}

} // namespace nixd::configuration
