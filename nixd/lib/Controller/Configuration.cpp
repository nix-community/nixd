#include "nixd/Controller/Controller.h"
#include "nixd/Eval/Launch.h"

#include <boost/asio/post.hpp>

using namespace nixd;
using namespace lspserver;
using llvm::json::ObjectMapper;
using llvm::json::Value;

bool nixd::fromJSON(const Value &Params, Configuration::Diagnostic &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("suppress", R.suppress);
}

bool nixd::fromJSON(const Value &Params, Configuration::Formatting &R,
                    llvm::json::Path P) {
  // If it is a single string, treat it as a single vector
  if (auto Str = Params.getAsString()) {
    R.command = {Str->str()};
    return true;
  }
  ObjectMapper O(Params, P);
  return O && O.mapOptional("command", R.command);
}

bool nixd::fromJSON(const Value &Params, Configuration::OptionProvider &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("expr", R.expr);
}

bool nixd::fromJSON(const Value &Params, Configuration::NixpkgsProvider &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("expr", R.expr);
}

bool nixd::fromJSON(const Value &Params, Configuration::SchemaDirective &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.mapOptional("enable", R.enable);
}

bool nixd::fromJSON(const Value &Params, Configuration &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O                                                      //
         && O.mapOptional("formatting", R.formatting)           //
         && O.mapOptional("options", R.options)                 //
         && O.mapOptional("nixpkgs", R.nixpkgs)                 //
         && O.mapOptional("diagnostic", R.diagnostic)           //
         && O.mapOptional("schemaDirective", R.schemaDirective) //
      ;
}

void Controller::onDidChangeConfiguration(
    const DidChangeConfigurationParams &Params) {
  // FIXME: incrementally change?
  fetchConfig();
}

void Controller::updateConfig(Configuration NewConfig) {
  std::unique_lock G(ConfigLock);
  Config = std::move(NewConfig);

  bool NixpkgsChanged = false;
  if (!Config.nixpkgs.expr.empty()) {
    NixpkgsChanged = NixpkgsExpr != Config.nixpkgs.expr;
    NixpkgsExpr = Config.nixpkgs.expr;
    /// Evaluate nixpkgs and options, using user-provided config.
    if (nixpkgsClient()) {
      evalExprWithProgress(*nixpkgsClient(), Config.nixpkgs.expr,
                           "nixpkgs entries");
    }
  }
  if (!Config.options.empty()) {
    std::lock_guard _(OptionsLock);
    // For each option configuration, update the worker.
    for (const auto &[Name, Opt] : Config.options) {
      auto &Client = Options[Name];
      if (!Client) {
        // If it does not exist. Launch a new client.
        Client = startOption(Name);
      }
      assert(Client);
      evalExprWithProgress(*Client->client(), Opt.expr, Name);
    }
  }

  // Update the diagnostic part.
  updateSuppressed(Config.diagnostic.suppress);

  // After all, notify all AST modules the diagnostic set has been updated.
  std::vector<std::pair<std::string, std::string>> Docs;
  {
    std::lock_guard TUsGuard(TUsLock);
    for (const auto &[File, TU] : TUs) {
      publishDiagnostics(File, std::nullopt, TU->src(), TU->diagnostics());
      Docs.emplace_back(File.str(), std::string(TU->src()));
    }
  }
  G.unlock();

  // Schema modules were evaluated against the old nixpkgs, and documents
  // opened before the client's configuration arrived were never resolved.
  if (NixpkgsChanged) {
    std::lock_guard _(DocOptionsLock);
    DocOptions.clear();
    DocOptionsStamp.clear();
  }
  for (const auto &[File, Src] : Docs)
    updateSchemaDirective(File, Src);
}

void Controller::fetchConfig() {
  auto Action = [this](llvm::Expected<llvm::json::Value> Resp) mutable {
    if (!Resp) {
      elog("workspace/configuration: {0}", Resp.takeError());
      return;
    }

    // LSP response is a json array, just take the first.
    if (Resp->kind() != llvm::json::Value::Array) {
      lspserver::elog("workspace/configuration response is not an array: {0}",
                      *Resp);
      return;
    }
    const Value &FirstConfig = Resp->getAsArray()->front();

    // Run this job in the thread pool. Don't block input thread.
    auto ConfigAction = [this, FirstConfig]() mutable {
      // Parse the config
      Configuration NewConfig;
      llvm::json::Path::Root P;
      if (!fromJSON(FirstConfig, NewConfig, P)) {
        elog("workspace/configuration: parse error {0}", P.getError());
        return;
      }

      // OK, update the config
      updateConfig(std::move(NewConfig));
    };

    boost::asio::post(Pool, std::move(ConfigAction));
  };
  workspaceConfiguration({.items = {ConfigurationItem{.section = "nixd"}}},
                         std::move(Action));
}

void Controller::workspaceConfiguration(
    const lspserver::ConfigurationParams &Params,
    lspserver::Callback<llvm::json::Value> Reply) {
  if (ClientCaps.WorkspaceConfiguration) {
    WorkspaceConfiguration(Params, std::move(Reply));
  } else {
    Reply(lspserver::error("client does not support workspace configuration"));
  }
}
