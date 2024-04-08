#pragma once

#include "AutoRemoveShm.h"

// boost
#include <boost/interprocess/mapped_region.hpp>

// stdc++
#include <memory>

namespace nixd::util {

struct OwnedRegion {
  std::unique_ptr<AutoRemoveShm> Shm;
  std::unique_ptr<boost::interprocess::mapped_region> Region;
};

} // namespace nixd::util
