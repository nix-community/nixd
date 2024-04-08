#include "nixd/Support/AutoRemoveShm.h"

namespace nixd::util {

AutoRemoveShm::AutoRemoveShm(std::string ShmName,
                             boost::interprocess::offset_t Size)
    : ShmName(std::move(ShmName)) {
  Shm = boost::interprocess::shared_memory_object(
      boost::interprocess::open_or_create, this->ShmName.c_str(),
      boost::interprocess::read_write);
  Shm.truncate(Size);
}

} // namespace nixd::util
