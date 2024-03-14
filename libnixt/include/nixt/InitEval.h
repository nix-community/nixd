#pragma once

#include <nix/eval.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>

namespace nixt {

inline void initEval() {
  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
  nix::initGC();
}

} // namespace nixt
