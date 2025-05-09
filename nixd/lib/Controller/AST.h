/// \file
/// \brief This file declares some common analysis (tree walk) on the AST.

#include "nixd/Protocol/AttrSet.h"

#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

#include <exception>

namespace nixd {

namespace idioms {

/// \brief Hardcoded name for "pkgs.xxx", or "with pkgs;"
///
/// Assume that the value of this variable have the same structure with `import
/// nixpkgs {}
constexpr inline std::string_view Pkgs = "pkgs";

/// \brief Hardcoded name for nixpkgs "lib"
///
/// Assume that the value of this variable is "nixpkgs lib".
/// e.g. lib.genAttrs.
constexpr inline std::string_view Lib = "lib";

struct IdiomException : std::exception {};

/// \brief Exceptions scoped in nixd::mkIdiomSelector
struct IdiomSelectorException : std::exception {};

/// \brief The pattern of this variable cannot be recognized by known idioms.
struct NotAnIdiomException : IdiomSelectorException {
  [[nodiscard]] const char *what() const noexcept override {
    return "not an idiom";
  }
};

struct VLAException : std::exception {};

struct NoLocationForBuiltinVariable : std::exception {
  [[nodiscard]] const char *what() const noexcept override {
    return "builtins are defined in the interpreter, not in the Nix files";
  }
};

/// \brief No such variable.
struct NoSuchVarException : VLAException {
  [[nodiscard]] const char *what() const noexcept override {
    return "no such variable";
  }
};

struct UndefinedVarException : VLAException {
  [[nodiscard]] const char *what() const noexcept override {
    return "undefined variable";
  }
};

/// \brief Construct a nixd::Selector from \p Var.
///
/// Try to heuristically find a selector of a variable, based on some known
/// idioms.
Selector mkVarSelector(const nixf::ExprVar &Var,
                       const nixf::VariableLookupAnalysis &VLA,
                       const nixf::ParentMapAnalysis &PM);

/// \brief The attrpath has a dynamic name, thus it cannot be trivially
/// transformed to "static" selector.
struct DynamicNameException : IdiomSelectorException {
  [[nodiscard]] const char *what() const noexcept override {
    return "dynamic attribute path encountered";
  }
};

struct NotVariableSelect : IdiomSelectorException {
  [[nodiscard]] const char *what() const noexcept override {
    return "the base expression of the select is not a variable";
  }
};

/// \brief Construct a nixd::Selector from \p AP.
Selector mkSelector(const nixf::AttrPath &AP, Selector BaseSelector);

/// \brief Construct a nixd::Selector from \p Select.
Selector mkSelector(const nixf::ExprSelect &Select, Selector BaseSelector);

/// \brief Construct a nixd::Selector from \p Select.
Selector mkSelector(const nixf::ExprSelect &Select,
                    const nixf::VariableLookupAnalysis &VLA,
                    const nixf::ParentMapAnalysis &PM);

} // namespace idioms

/// \brief Search up until there are some node associated with "EnvNode".
[[nodiscard]] const nixf::EnvNode *
upEnv(const nixf::Node &Desc, const nixf::VariableLookupAnalysis &VLA,
      const nixf::ParentMapAnalysis &PM);

/// \brief Determine whether or not some node has enclosed "with pkgs; [ ]"
///
/// Yes, this evaluation isn't flawless. What if the identifier isn't "pkgs"? We
/// can't dynamically evaluate everything each time and invalidate them
/// immediately after document updates. Therefore, this heuristic method
/// represents a trade-off between performance considerations.
[[nodiscard]] bool havePackageScope(const nixf::Node &N,
                                    const nixf::VariableLookupAnalysis &VLA,
                                    const nixf::ParentMapAnalysis &PM);

/// \brief get variable scope, and it's prefix name.
///
/// Nixpkgs has some packages scoped in "nested" attrs.
/// e.g. llvmPackages, pythonPackages.
/// Try to find these name as a pre-selected scope, the last value is "prefix".
std::pair<std::vector<std::string>, std::string>
getScopeAndPrefix(const nixf::Node &N, const nixf::ParentMapAnalysis &PM);

enum class FindAttrPathResult {
  OK,
  Inherit,
  NotAttrPath,
  WithDynamic,
};

/// \brief Heuristically find attrpath suitable for "attrpath" completion.
/// \param[out] Path the attrpath.
FindAttrPathResult findAttrPath(const nixf::Node &N,
                                const nixf::ParentMapAnalysis &PM,
                                std::vector<std::string> &Path);

/// \brief Heuristically find attrpath suitable for "attrpath" completion.
/// Strips "config." from the start to support config sections in NixOS modules.
///
/// \param[out] Path the attrpath.
FindAttrPathResult findAttrPathForOptions(const nixf::Node &N,
                                          const nixf::ParentMapAnalysis &PM,
                                          std::vector<std::string> &Path);

} // namespace nixd
