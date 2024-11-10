#pragma once

#include <nix/common-eval-args.hh>
#include <nix/eval-gc.hh>
#include <nix/eval-settings.hh>
#include <nix/eval.hh>
#include <nix/flake/flake.hh>
#include <nix/plugin.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>

namespace nixt {

inline void initEval() {
  nix::initNix();
  nix::initLibStore();
  nix::flake::initLib(nix::flakeSettings);
  nix::initPlugins();
  nix::initGC();
}

} // namespace nixt
