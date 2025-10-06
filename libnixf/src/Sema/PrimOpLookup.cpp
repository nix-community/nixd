#include "nixf/Sema/PrimOpInfo.h"

using namespace nixf;

PrimopLookupResult nixf::lookupGlobalPrimOpInfo(const std::string &Name) {
  if (PrimOpsInfo.contains(Name)) {
    return PrimopLookupResult::Found;
  }

  // Prefix the name with "__", and check again.
  std::string PrefixedName = "__" + Name;
  if (PrimOpsInfo.contains(PrefixedName)) {
    return PrimopLookupResult::PrefixedFound;
  }

  return PrimopLookupResult::NotFound;
}
