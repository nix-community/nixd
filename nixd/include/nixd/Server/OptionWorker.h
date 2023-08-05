#pragma once

#include "nixd/Server/Config.h"
#include "nixd/Server/EvalDraftStore.h"
#include "nixd/Server/IPC.h"

#include "lspserver/LSPServer.h"

#include <nix/value.hh>

namespace nixd {

class OptionWorker : public lspserver::LSPServer {
private:
  /// The AttrSet having options, we use this for any nixpkgs options.
  /// nixpkgs basically defined options under "options" attrpath
  /// we can use this for completion (to support ALL option system)
  nix::Value *OptionAttrSet;
  std::unique_ptr<IValueEvalSession> OptionIES;

public:
  OptionWorker(std::unique_ptr<lspserver::InboundPort> In,
               std::unique_ptr<lspserver::OutboundPort> Out);

  void onEvalOptionSet(const configuration::TopLevel::Options &Config);

  void onDeclaration(const ipc::AttrPathParams &,
                     lspserver::Callback<lspserver::Location>);

  void onCompletion(const ipc::AttrPathParams &,
                    lspserver::Callback<llvm::json::Value>);
};

} // namespace nixd
