#pragma once

#include "nixd/Support/AutoCloseFD.h"

#include <functional>

namespace nixd::detail {

void closeOwnedWith(util::AutoCloseFD &FD,
                    const std::function<int(int)> &Close) noexcept;

} // namespace nixd::detail
