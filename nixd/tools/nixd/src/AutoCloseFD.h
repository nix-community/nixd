#pragma once

#include <cerrno>
#include <optional>

namespace nixd {

/// \brief File Descriptor RAII wrapper
class AutoCloseFD {
public:
  using FDTy = int;

private:
  static constexpr FDTy ReleasedFD = -EBADF;
  FDTy FD;

public:
  AutoCloseFD(FDTy FD);
  AutoCloseFD(const AutoCloseFD &) = delete;
  AutoCloseFD(AutoCloseFD &&That) noexcept;

  [[nodiscard]] bool isReleased() const;
  static bool isReleased(FDTy FD);

  ~AutoCloseFD();

  [[nodiscard]] FDTy get() const;
  void release();
};

} // namespace nixd
