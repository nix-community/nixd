/// Access ParseCache in \p nix::EvalState

#pragma once

#include <nix/eval.hh>

namespace nixt {

#if HAVE_BOEHMGC
using FileParseCache = std::map<
    nix::SourcePath, nix::Expr *, std::less<nix::SourcePath>,
    traceable_allocator<std::pair<const nix::SourcePath, nix::Expr *>>>;
#else
using FileParseCache = std::map<nix::SourcePath, nix::Expr *>;
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

struct ParseCacheF {
  using type = FileParseCache nix::EvalState::*;
};

template struct RB<ParseCacheF, &nix::EvalState::fileParseCache>;

} // namespace detail

inline FileParseCache &getFileParseCache(nix::EvalState &S) {
  return S.*detail::R<detail::ParseCacheF>::P;
}

} // namespace nixt
