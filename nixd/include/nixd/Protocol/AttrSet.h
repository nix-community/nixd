/// \file
/// \brief Types used in nixpkgs provider.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <llvm/Support/JSON.h>
#include <lspserver/Protocol.h>

namespace nixd {

namespace rpcMethod {

constexpr inline std::string_view EvalExpr = "attrset/evalExpr";
constexpr inline std::string_view AttrPathInfo = "attrset/attrpathInfo";
constexpr inline std::string_view AttrPathComplete = "attrset/attrpathComplete";
constexpr inline std::string_view OptionInfo = "attrset/optionInfo";
constexpr inline std::string_view OptionComplete = "attrset/optionComplete";
constexpr inline std::string_view Exit = "exit";

} // namespace rpcMethod

using EvalExprParams = std::string;
using EvalExprResponse = std::optional<std::string>;

/// \brief A list of strings that "select"s into a attribute set.
using Selector = std::vector<std::string>;

using PackageInfoParams = Selector;
using OptionInfoParams = Selector;

using AttrPathInfoParams = Selector;

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
  Selector Scope;
  /// \brief Search for packages prefixed with this "prefix"
  std::string Prefix;
};

llvm::json::Value toJSON(const AttrPathCompleteParams &Params);
bool fromJSON(const llvm::json::Value &Params, AttrPathCompleteParams &R,
              llvm::json::Path P);

using AttrPathCompleteResponse = std::vector<std::string>;

struct OptionType {
  std::optional<std::string> Description;
  std::optional<std::string> Name;
};

llvm::json::Value toJSON(const OptionType &Params);
bool fromJSON(const llvm::json::Value &Params, OptionType &R,
              llvm::json::Path P);

struct OptionDescription {
  std::optional<std::string> Description;
  std::vector<lspserver::Location> Declarations;
  std::vector<lspserver::Location> Definitions;
  std::optional<std::string> Example;
  std::optional<OptionType> Type;
};

llvm::json::Value toJSON(const OptionDescription &Params);
bool fromJSON(const llvm::json::Value &Params, OptionDescription &R,
              llvm::json::Path P);

struct OptionField {
  std::string Name;
  std::optional<OptionDescription> Description;
};

llvm::json::Value toJSON(const OptionField &Params);
bool fromJSON(const llvm::json::Value &Params, OptionField &R,
              llvm::json::Path P);

using OptionInfoResponse = OptionDescription;

using OptionCompleteResponse = std::vector<OptionField>;

} // namespace nixd
