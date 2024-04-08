/// Access EvalCache in \p nix::EvalState

#pragma once

#include <nix/eval.hh>

namespace nixt {

#if HAVE_BOEHMGC
using FileEvalCache =
    std::map<nix::SourcePath, nix::Value, std::less<nix::SourcePath>,
             traceable_allocator<std::pair<const nix::SourcePath, nix::Value>>>;
#else
using FileEvalCache = std::map<nix::SourcePath, nix::Expr *>;
#endif

namespace detail {

template <typename Tag> struct R {
  using type = typename Tag::type;
  static type P;
};

template <typename Tag> typename R<Tag>::type R<Tag>::P;

template <typename Tag, typename Tag::type p> struct RB : R<Tag> {
  struct F {
    F() { R<Tag>::P = p; }
  };
  static F FO;
};

template <typename Tag, typename Tag::type p>
typename RB<Tag, p>::F RB<Tag, p>::FO;

// Impl

struct EvalCacheF {
  using type = FileEvalCache nix::EvalState::*;
};

template struct RB<EvalCacheF, &nix::EvalState::fileEvalCache>;

} // namespace detail

inline FileEvalCache &getFileEvalCache(nix::EvalState &S) {
  return S.*detail::R<detail::EvalCacheF>::P;
}

} // namespace nixt
