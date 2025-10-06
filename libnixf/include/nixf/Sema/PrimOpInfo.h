#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nixf {

/**
 * Info about a primitive operation, but omit the implementation.
 */
struct PrimOpInfo {
  /**
   * Names of the parameters of a primop, for primops that take a
   * fixed number of arguments to be substituted for these parameters.
   */
  std::vector<std::string> Args;

  /**
   * Aritiy of the primop.
   *
   * If `args` is not empty, this field will be computed from that
   * field instead, so it doesn't need to be manually set.
   */
  size_t Arity = 0;

  /**
   * Optional free-form documentation about the primop.
   */
  std::string Doc;

  /**
   * If true, this primop is not exposed to the user.
   */
  bool Internal;
};

extern std::map<std::string, nixf::PrimOpInfo> PrimOpsInfo;

/// \brief Result of looking up a primop by name.
enum class PrimopLookupResult : std::uint8_t {
  /// The primop was found with an exact match.
  Found,
  /// The primop was found, but needs "builtin." prefix.
  PrefixedFound,
  /// The primop was not found.
  NotFound,
};

/// \brief Look up information about a global primop by name.
PrimopLookupResult lookupGlobalPrimOpInfo(const std::string &Name);

} // namespace nixf
