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

  if (!checkObject(Params, P,
                   [](llvm::StringRef Key) { return Key == "command"; }))
    return false;
  ObjectMapper Mapper(Params, P);
  return Mapper && Mapper.mapOptional("command", R.command);
}

bool nixd::fromJSON(const Value &Params, ConfigurationPatch::NixpkgsProvider &R,
                    llvm::json::Path P) {
  if (!checkObject(Params, P,
                   [](llvm::StringRef Key) { return Key == "expr"; }))
    return false;
  ObjectMapper Mapper(Params, P);
  return Mapper && Mapper.mapOptional("expr", R.expr);
}

bool nixd::fromJSON(const Value &Params, ConfigurationPatch::Diagnostic &R,
                    llvm::json::Path P) {
  if (!checkObject(Params, P,
                   [](llvm::StringRef Key) { return Key == "suppress"; }))
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
      "nixos",
      Configuration::OptionProvider{
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

ProviderSpec nixd::providerSpec(const Configuration &Config) {
  ProviderSpec Spec;
  if (!Config.nixpkgs.expr.empty())
    Spec.Nixpkgs = Config.nixpkgs.expr;
  for (const auto &[Name, Option] : Config.options) {
    if (!Option.expr.empty())
      Spec.Options.emplace(Name, Option.expr);
  }
  return Spec;
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
  fetchConfig();
}

void Controller::updateConfig(Configuration NewConfig,
                              ProviderRegistry::ApplyCallback OnApplied) {
  if (!Accepting)
    return;
  boost::asio::post(ConfigStrand, [this, NewConfig = std::move(NewConfig),
                                   OnApplied = std::move(OnApplied)]() mutable {
    applyConfig(std::move(NewConfig), std::move(OnApplied));
  });
}

void Controller::applyConfig(Configuration NewConfig,
                             ProviderRegistry::ApplyCallback OnApplied) {
  if (!Accepting || !Providers) {
    if (OnApplied)
      OnApplied(Accepting ? ProviderApplyResult::Failed
                          : ProviderApplyResult::Stopped);
    return;
  }

  const ProviderSpec Spec = providerSpec(NewConfig);
  const auto Suppressed = NewConfig.diagnostic.suppress;

  // Publish provider revisions before the corresponding configuration can be
  // observed. Once Config exposes the new expressions, no query may still
  // validate a token from the previous provider snapshot.
  Providers->apply(Spec, std::move(OnApplied));

  {
    std::lock_guard Guard(ConfigLock);
    Config = std::move(NewConfig);
  }

  // Update the diagnostic part.
  updateSuppressed(Suppressed);

  // After all, notify all AST modules the diagnostic set has been updated.
  std::lock_guard TUsGuard(TUsLock);
  for (const auto &[File, TU] : TUs) {
    publishDiagnostics(File, std::nullopt, TU->src(), TU->diagnostics());
  }
}

void Controller::fetchConfig() {
  if (!Accepting || !EditorConfig)
    return;
  auto Generation = EditorConfig->issue();
  if (!Generation)
    return;

  // The callback retains only the editor's shared strand state. The complete
  // Expected<Value> is moved off the LSP input callback before it is parsed.
  auto Editor = *EditorConfig;
  auto Action = [Editor = std::move(Editor), Generation = *Generation](
                    llvm::Expected<llvm::json::Value> Response) mutable {
    Editor.submit(Generation, std::move(Response));
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
