#pragma once

#include <nix/eval.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>

namespace nixd {

class InitNix {
public:
  InitNix() {
    nix::initNix();
    nix::initGC();
  }
  std::unique_ptr<nix::EvalState> getDummyState() {
    return std::make_unique<nix::EvalState>(nix::Strings{},
                                            nix::openStore("dummy://"));
  }
};
} // namespace nixd
