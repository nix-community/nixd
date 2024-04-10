/// \file
/// \brief This file declares some common analysis (tree walk) on the AST.

#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

namespace nixd {

/// \brief Search up until there are some node associated with "EnvNode".
[[nodiscard]] const nixf::EnvNode *
upEnv(const nixf::Node &Desc, const nixf::VariableLookupAnalysis &VLA,
      const nixf::ParentMapAnalysis &PM);

/// \brief Determine whether or not some node has enclosed "with pkgs; [ ]"
///
/// Yes. this is not perfectly evaluated. what about the identifier is not
/// "pkgs"? We cannot eval all things dynamically, each time, and invalidate
/// them once after document updates. So this heuristic method is somehow result
/// of trade off between performance.
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

} // namespace nixd
