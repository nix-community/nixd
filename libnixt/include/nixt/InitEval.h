#pragma once

#if NIX_IMPL == 1
#include <nix/cmd/common-eval-args.hh>
#include <nix/expr/eval-gc.hh>
#include <nix/expr/eval-settings.hh>
#include <nix/expr/eval.hh>
#include <nix/flake/flake.hh>
#include <nix/flake/settings.hh>
#include <nix/main/plugin.hh>
#include <nix/main/shared.hh>
#include <nix/store/store-api.hh>
#else
#include <flake/flake.hh>
#include <shared.hh>
#include <store-api.hh>
#endif

namespace nixt {

inline void initEval() {
  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
#if NIX_IMPL == 1
  nix::flakeSettings.configureEvalSettings(nix::evalSettings);
  nix::initGC();
#endif
}

} // namespace nixt
