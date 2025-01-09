#pragma once

/// \brief Used for simplify early-returns.
///
///   const auto *foo = checkReturn(get(), nullptr)
#define CheckReturn(x, Ret)                                                    \
  ({                                                                           \
    decltype(x) temp = (x);                                                    \
    if (!temp) [[unlikely]] {                                                  \
      return Ret;                                                              \
    }                                                                          \
    temp;                                                                      \
  })

/// \brief Variant of `CheckReturn`, but returns default constructed `CheckTy`
#define CheckDefault(x) CheckReturn(x, CheckTy{})
