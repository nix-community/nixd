#pragma once

#include <boost/interprocess/shared_memory_object.hpp>

namespace nixd::util {

/// Shared memory object, the object will be removed in dtor.
class AutoRemoveShm {
  boost::interprocess::shared_memory_object Shm;
  std::string ShmName;

public:
  AutoRemoveShm(std::string ShmName, boost::interprocess::offset_t Size);

  ~AutoRemoveShm() { Shm.remove(ShmName.c_str()); }

  [[nodiscard]] const std::string &shmName() const { return ShmName; }

  boost::interprocess::shared_memory_object &get() { return Shm; }
};

} // namespace nixd::util
