#pragma once

#include "lspserver/Function.h"
#include "lspserver/Logger.h"

namespace nixd {

template <class ReplyTy> struct ReplyRAII {
  lspserver::Callback<ReplyTy> R;
  llvm::Expected<ReplyTy> Response = lspserver::error("no response available");
  ReplyRAII(decltype(R) R) : R(std::move(R)) {}
  ~ReplyRAII() {
    if (R)
      R(std::move(Response));
  };
  ReplyRAII(ReplyRAII &&Old) noexcept {
    R = std::move(Old.R);
    Response = std::move(Old.Response);
  }
};

} // namespace nixd
