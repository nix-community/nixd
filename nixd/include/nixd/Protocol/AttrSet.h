/// \file
/// \brief Types used in nixpkgs provider.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <llvm/Support/JSON.h>

namespace nixd {

using EvalExprParams = std::string;
using EvalExprResponse = std::optional<std::string>;

using AttrPathInfoParams = std::vector<std::string>;

struct PackageDescription {
  std::optional<std::string> Name;
  std::optional<std::string> PName;
  std::optional<std::string> Version;
  std::optional<std::string> Description;
  std::optional<std::string> LongDescription;
  std::optional<std::string> Position;
  std::optional<std::string> Homepage;
};

using AttrPathInfoResponse = PackageDescription;

llvm::json::Value toJSON(const PackageDescription &Params);
bool fromJSON(const llvm::json::Value &Params, PackageDescription &R,
              llvm::json::Path P);

struct AttrPathCompleteParams {
  std::vector<std::string> Scope;
  /// \brief Search for packages prefixed with this "prefix"
  std::string Prefix;
};

llvm::json::Value toJSON(const AttrPathCompleteParams &Params);
bool fromJSON(const llvm::json::Value &Params, AttrPathCompleteParams &R,
              llvm::json::Path P);

using AttrPathCompleteResponse = std::vector<std::string>;

} // namespace nixd
