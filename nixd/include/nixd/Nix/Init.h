#pragma once

#include <nix/eval.hh>
#include <nix/globals.hh>
#include <nix/shared.hh>

namespace nixd {

inline void initEval() {
  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
  nix::initGC();
}

} // namespace nixd
