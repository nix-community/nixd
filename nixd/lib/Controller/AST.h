/// \file
/// \brief This file declares some common analysis (tree walk) on the AST.

#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

#include <optional>

namespace nixd {

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

/// \brief Find nested attrpath.
/// e.g.   a.b.c.d
///               ^<-  a.b.c.d
/// { a = 1; b = { c = d; }; }
///                 ^ b.c
///
/// SelectAttrPath := { a.b.c.d = 1; }
///                     ^~~~~~~<--------- such "selection"
/// ValueAttrPath := N is a "value", find how it's nested.
std::vector<std::string_view>
getValueAttrPath(const nixf::Node &N, const nixf::ParentMapAnalysis &PM);

/// \copydoc getValueAttrPath
std::vector<std::string_view>
getSelectAttrPath(const nixf::AttrName &N, const nixf::ParentMapAnalysis &PM);

/// \brief Heuristically find attrpath suitable for "attrpath" completion.
/// \returns non-empty std::vector attrpath.
std::optional<std::vector<std::string_view>>
findAttrPath(const nixf::Node &N, const nixf::ParentMapAnalysis &PM);

} // namespace nixd
