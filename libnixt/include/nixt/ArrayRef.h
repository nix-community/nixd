/// \file
/// \brief `ArrayRef`, `BytesRef`, and related functions.
#pragma once

#include <string_view>

namespace nixt {

/// \brief Weak reference to an array, with begin and end pointers.
/// \note Please always pass/return by value and don't add member functions.
template <class T> struct ArrayRef {
  const T *Begin;
  const T *End;
};

using BytesRef = ArrayRef<char>;

/// \brief Iterator begin. Used for `range-based-for`
template <class T> inline const T *begin(ArrayRef<T> B) { return B.Begin; }

/// \brief Iterator end.
template <class T> inline const T *end(ArrayRef<T> B) { return B.End; }

inline std::string_view view(BytesRef B) { return {B.Begin, B.End}; }

/// \brief Advance the beginning pointer of bytes array.
template <class T> inline ArrayRef<T> advance(ArrayRef<T> B, long Offset) {
  return {B.Begin + Offset, B.End};
}
/// \brief Get length of this array.
template <class T> inline std::size_t lengthof(ArrayRef<T> B) {
  return B.End - B.Begin;
}

} // namespace nixt
