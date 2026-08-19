#include "nixd/Controller/Controller.h"
#include "nixd/Eval/Launch.h"

#include <boost/asio/post.hpp>

using namespace nixd;
using namespace lspserver;
using llvm::json::ObjectMapper;
using llvm::json::Value;

namespace {

template <typename IsKnown>
bool checkObject(const Value &Params, llvm::json::Path P, IsKnown Known) {
  const auto *Object = Params.getAsObject();
  if (!Object) {
    P.report("expected object");
    return false;
  }

  for (const auto &Entry : *Object) {
    if (!Known(Entry.first)) {
      P.field(Entry.first).report("unknown configuration field");
      return false;
    }
    if (Entry.second.kind() == Value::Null) {
      P.field(Entry.first).report("null configuration fields are not allowed");
      return false;
    }
  }
  return true;
}

} // namespace

bool nixd::fromJSON(const Value &Params, ConfigurationPatch::Formatting &R,
                    llvm::json::Path P) {
  if (auto String = Params.getAsString()) {
    if (String->empty()) {
      P.report("formatting command must not be empty");
      return false;
    }
    R.command = std::vector<std::string>{String->str()};
    return true;
  }

  if (!checkObject(Params, P, [](llvm::StringRef Key) {
        return Key == "command";
      }))
    return false;
  ObjectMapper Mapper(Params, P);
  return Mapper && Mapper.mapOptional("command", R.command);
}

bool nixd::fromJSON(const Value &Params,
                    ConfigurationPatch::NixpkgsProvider &R,
                    llvm::json::Path P) {
  if (!checkObject(Params, P, [](llvm::StringRef Key) {
        return Key == "expr";
      }))
    return false;
  ObjectMapper Mapper(Params, P);
  return Mapper && Mapper.mapOptional("expr", R.expr);
}

bool nixd::fromJSON(const Value &Params, ConfigurationPatch::Diagnostic &R,
                    llvm::json::Path P) {
  if (!checkObject(Params, P, [](llvm::StringRef Key) {
        return Key == "suppress";
      }))
    return false;
  ObjectMapper Mapper(Params, P);
  return Mapper && Mapper.mapOptional("suppress", R.suppress);
}

bool nixd::fromJSON(const Value &Params, Configuration::OptionProvider &R,
                    llvm::json::Path P) {
  if (!checkObject(Params, P,
                   [](llvm::StringRef Key) { return Key == "expr"; }))
    return false;

  std::optional<std::string> Expr;
  ObjectMapper Mapper(Params, P);
  if (!Mapper || !Mapper.mapOptional("expr", Expr))
    return false;
  if (!Expr || Expr->empty()) {
    P.field("expr").report("expected non-empty string");
    return false;
  }
  R.expr = std::move(*Expr);
  return true;
}

Configuration nixd::defaultConfiguration() {
  Configuration Config;
  Config.nixpkgs.expr = "import <nixpkgs> { }";
  Config.options.emplace(
      "nixos", Configuration::OptionProvider{
                   "(let pkgs = import <nixpkgs> { }; in (pkgs.lib.evalModules "
                   "{ modules =  (import <nixpkgs/nixos/modules/module-list.nix>) "
                   "++ [ ({...}: { nixpkgs.hostPlatform = "
                   "builtins.currentSystem;} ) ] ; })).options"});
  return Config;
}

Configuration nixd::overlay(Configuration Base,
                            const ConfigurationPatch &Patch) {
  if (Patch.formatting && Patch.formatting->command)
    Base.formatting.command = *Patch.formatting->command;
  if (Patch.options)
    Base.options = *Patch.options;
  if (Patch.nixpkgs && Patch.nixpkgs->expr)
    Base.nixpkgs.expr = *Patch.nixpkgs->expr;
  if (Patch.diagnostic && Patch.diagnostic->suppress)
    Base.diagnostic.suppress = *Patch.diagnostic->suppress;
  return Base;
}

bool nixd::fromJSON(const Value &Params, ConfigurationPatch &R,
                    llvm::json::Path P) {
  if (!checkObject(Params, P, [](llvm::StringRef Key) {
        return Key == "$schema" || Key == "formatting" || Key == "options" ||
               Key == "nixpkgs" || Key == "diagnostic";
      }))
    return false;

  std::string Schema;
  ObjectMapper Mapper(Params, P);
  return Mapper && Mapper.mapOptional("$schema", Schema) &&
         Mapper.mapOptional("formatting", R.formatting) &&
         Mapper.mapOptional("options", R.options) &&
         Mapper.mapOptional("nixpkgs", R.nixpkgs) &&
         Mapper.mapOptional("diagnostic", R.diagnostic);
}

void Controller::onDidChangeConfiguration(
    const DidChangeConfigurationParams &Params) {
  // FIXME: incrementally change?
  fetchConfig();
}

void Controller::updateConfig(Configuration NewConfig) {
  std::lock_guard G(ConfigLock);
  Config = std::move(NewConfig);

  if (!Config.nixpkgs.expr.empty()) {
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
        assert(Startup);
        startOption(Name, Client, Startup->executionCWD);
      }
      assert(Client);
      evalExprWithProgress(*Client->client(), Opt.expr, Name);
    }
  }

  // Update the diagnostic part.
  updateSuppressed(Config.diagnostic.suppress);

  // After all, notify all AST modules the diagnostic set has been updated.
  std::lock_guard TUsGuard(TUsLock);
  for (const auto &[File, TU] : TUs) {
    publishDiagnostics(File, std::nullopt, TU->src(), TU->diagnostics());
  }
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
      // Parse and apply the editor patch over the current configuration.
      ConfigurationPatch Patch;
      llvm::json::Path::Root P;
      if (!fromJSON(FirstConfig, Patch, P)) {
        elog("workspace/configuration: parse error {0}", P.getError());
        return;
      }

      assert(Startup);
      Configuration Base = Startup->baseConfiguration;

      // OK, update the config
      updateConfig(overlay(std::move(Base), Patch));
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
