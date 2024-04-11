/// \file
/// \brief Dedicated worker for evaluating attrset.
///
/// Motivation: eval things in attrset (e.g. nixpkgs). (packages, functions ...)
///
/// Observation:
/// 1. Fast: eval (import <nixpkgs> { }).
/// 2. Slow: eval a whole NixOS config until some node is being touched.
///
/// This worker is designed to answer "packages"/"functions" in nixpkgs, but not
/// limited to it.
///
/// For time-saving:
/// Once a value is evaluated. It basically assume it will not change.
/// That is, any workspace editing will not invalidate the value.
///

#pragma once

#include "nixd/Protocol/AttrSet.h"

#include "lspserver/LSPServer.h"

#include <nix/eval.hh>

#include <memory>

namespace nixd {

/// \brief Main RPC class for attrset provider.
class AttrSetProvider : public lspserver::LSPServer {

  std::unique_ptr<nix::EvalState> State;

  nix::Value Nixpkgs;

  /// Convenient method for get state. Basically assume this->State is not null
  nix::EvalState &state() {
    assert(State && "State should be allocated by ctor!");
    return *State;
  }

public:
  AttrSetProvider(std::unique_ptr<lspserver::InboundPort> In,
                  std::unique_ptr<lspserver::OutboundPort> Out);

  /// \brief Eval an expression, use it for furthur requests.
  void onEvalExpr(const EvalExprParams &Name,
                  lspserver::Callback<EvalExprResponse> Reply);

  /// \brief Query attrpath information.
  void onAttrPathInfo(const AttrPathInfoParams &AttrPath,
                      lspserver::Callback<AttrPathInfoResponse> Reply);

  /// \brief Complete attrpath entries.
  void onAttrPathComplete(const AttrPathCompleteParams &Params,
                          lspserver::Callback<AttrPathCompleteResponse> Reply);
};

} // namespace nixd
