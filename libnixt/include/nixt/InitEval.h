#pragma once

#include <nix/cmd/common-eval-args.hh>
#include <nix/expr/eval-gc.hh>
#include <nix/expr/eval-settings.hh>
#include <nix/expr/eval.hh>
#include <nix/flake/flake.hh>
#include <nix/flake/settings.hh>
#include <nix/main/plugin.hh>
#include <nix/main/shared.hh>
#include <nix/store/store-api.hh>

namespace nixt {

inline void initEval() {
  nix::initNix();
  nix::initLibStore();
  nix::flakeSettings.configureEvalSettings(nix::evalSettings);
  nix::initPlugins();
  nix::initGC();
}

} // namespace nixt
