#include "nixf/Sema/VariableLookup.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Lambda.h"

#include "nixd/Eval/AttrSetClient.h"
#include "nixd/Eval/Spawn.h"
#include "nixd/Protocol/AttrSet.h"

#include <llvm/Support/Error.h>

#include <array>
#include <semaphore>
#include <string_view>

using namespace nixf;

namespace {

/// Builder a map of definitions. If there are something overlapped, maybe issue
/// a diagnostic.
class DefBuilder {
  EnvNode::DefMap Def;

public:
  void addBuiltin(std::string Name) {
    // Don't need to record def map for builtins.
    auto _ = add(std::move(Name), nullptr, Definition::DS_Builtin);
  }

  [[nodiscard("Record ToDef Map!")]] std::shared_ptr<Definition>
  add(std::string Name, const Node *Entry,
      Definition::DefinitionSource Source) {
    assert(!Def.contains(Name));
    auto NewDef = std::make_shared<Definition>(Entry, Source);
    Def.insert({std::move(Name), NewDef});
    return NewDef;
  }

  EnvNode::DefMap finish() { return std::move(Def); }
};

} // namespace

bool EnvNode::isLive() const {
  for (const auto &[_, D] : Defs) {
    if (!D->uses().empty())
      return true;
  }
  return false;
}

void VariableLookupAnalysis::emitEnvLivenessWarning(
    const std::shared_ptr<EnvNode> &NewEnv) {
  for (const auto &[Name, Def] : NewEnv->defs()) {
    // If the definition comes from lambda arg, omit the diagnostic
    // because there is no elegant way to "fix" this trivially & keep
    // the lambda signature.
    if (Def->source() == Definition::DS_LambdaArg)
      continue;
    // Ignore builtins usage.
    if (!Def->syntax())
      continue;
    if (Def->uses().empty()) {
      Diagnostic::DiagnosticKind Kind = [&]() {
        switch (Def->source()) {
        case Definition::DS_Let:
          return Diagnostic::DK_UnusedDefLet;
        case Definition::DS_LambdaNoArg_Formal:
          return Diagnostic::DK_UnusedDefLambdaNoArg_Formal;
        case Definition::DS_LambdaWithArg_Formal:
          return Diagnostic::DK_UnusedDefLambdaWithArg_Formal;
        case Definition::DS_LambdaWithArg_Arg:
          return Diagnostic::DK_UnusedDefLambdaWithArg_Arg;
        default:
          assert(false && "liveness diagnostic encountered an unknown source!");
          __builtin_unreachable();
        }
      }();
      Diagnostic &D = Diags.emplace_back(Kind, Def->syntax()->range());
      D << Name;
      D.tag(DiagnosticTag::Faded);
    }
  }
}

static constexpr std::array<std::string_view, 2> KnownIdioms = {
    "pkgs",
    "lib",
};

std::string joinScope(const std::vector<std::string> &Scope) {
  std::string Key;
  bool First = true;
  for (const auto &Part : Scope) {
    if (Part.empty())
      continue;
    if (!First)
      Key.push_back('.');
    First = false;
    Key.append(Part);
  }
  return Key;
}

void VariableLookupAnalysis::lookupVar(const ExprVar &Var,
                                       const std::shared_ptr<EnvNode> &Env) {
  const auto &Name = Var.id().name();
  const auto *CurEnv = Env.get();
  std::shared_ptr<Definition> Def;
  std::vector<const EnvNode *> WithEnvs;
  for (; CurEnv; CurEnv = CurEnv->parent()) {
    if (CurEnv->defs().contains(Name)) {
      Def = CurEnv->defs().at(Name);
      break;
    }
    // Find all nested "with" expression, variables potentially come from those.
    // For example
    //    with lib;
    //        with builtins;
    //            generators <--- this variable may come from "lib" | "builtins"
    //
    // In general, we cannot determine where it precisely come from, thus mark
    // all Envs alive. (We'll make an attempt to resolve the variable with
    // nixpkgs client below.)
    if (CurEnv->isWith()) {
      WithEnvs.emplace_back(CurEnv);
    }
  }

  if (Def) {
    Def->usedBy(Var);
    Results.insert({&Var, LookupResult{LookupResultKind::Defined, Def}});
  } else if (!WithEnvs.empty()) { // comes from enclosed "with" expressions.
    for (const auto *WithEnv : WithEnvs) {
      Def = WithDefs.at(WithEnv->syntax());
      Def->usedBy(Var);

      // Attempt to resolve the variable with nixpkgs client.
      bool IsKnown = false;
      const auto &With = static_cast<const ExprWith &>(*WithEnv->syntax());

      auto Selector = With.selector();
      if (Selector.empty())
        continue;
      for (std::string_view Idiom : KnownIdioms) {
        if (Selector.front() != Idiom)
          continue;
        if (const auto *Known = ensureNixpkgsKnownAttrsCached(Selector))
          IsKnown = Known->contains(Name);
        break;
      }
      if (IsKnown)
        break;
    }
    Results.insert({&Var, LookupResult{LookupResultKind::FromWith, Def}});
  } else {
    // Otherwise, this variable is undefined.
    Results.insert({&Var, LookupResult{LookupResultKind::Undefined, nullptr}});
    Diagnostic &Diag =
        Diags.emplace_back(Diagnostic::DK_UndefinedVariable, Var.range());
    Diag << Var.id().name();
  }
}

void VariableLookupAnalysis::dfs(const ExprLambda &Lambda,
                                 const std::shared_ptr<EnvNode> &Env) {
  // Early exit for in-complete lambda.
  if (!Lambda.body())
    return;

  // Create a new EnvNode, as lambdas may have formal & arg.
  DefBuilder DBuilder;
  assert(Lambda.arg());
  const LambdaArg &Arg = *Lambda.arg();

  // foo: body
  // ^~~<------- add function argument.
  if (Arg.id()) {
    if (!Arg.formals()) {
      ToDef.insert_or_assign(Arg.id(), DBuilder.add(Arg.id()->name(), Arg.id(),
                                                    Definition::DS_LambdaArg));
      // Function arg cannot duplicate to it's formal.
      // If it this unluckily happens, we would like to skip this definition.
    } else if (!Arg.formals()->dedup().contains(Arg.id()->name())) {
      ToDef.insert_or_assign(Arg.id(),
                             DBuilder.add(Arg.id()->name(), Arg.id(),
                                          Definition::DS_LambdaWithArg_Arg));
    }
  }

  // { foo, bar, ... } : body
  //   ^~~~~~~~~<--------------  add function formals.

  // This section differentiates between formal parameters with an argument and
  // without. Example:
  //
  //  { foo }@arg : use arg
  //
  // In this case, the definition of `foo` is not used directly; however, it
  // might be accessed via arg.foo. Therefore, the severity of an unused formal
  // parameter is reduced in this scenario.
  if (Arg.formals()) {
    for (const auto &[Name, Formal] : Arg.formals()->dedup()) {
      Definition::DefinitionSource Source =
          Arg.id() ? Definition::DS_LambdaWithArg_Formal
                   : Definition::DS_LambdaNoArg_Formal;
      ToDef.insert_or_assign(Formal->id(),
                             DBuilder.add(Name, Formal->id(), Source));
    }
  }

  auto NewEnv = std::make_shared<EnvNode>(Env, DBuilder.finish(), &Lambda);

  if (Arg.formals()) {
    for (const auto &Formal : Arg.formals()->members()) {
      if (const Expr *Def = Formal->defaultExpr()) {
        dfs(*Def, NewEnv);
      }
    }
  }

  dfs(*Lambda.body(), NewEnv);

  emitEnvLivenessWarning(NewEnv);
}

void VariableLookupAnalysis::dfsDynamicAttrs(
    const std::vector<Attribute> &DynamicAttrs,
    const std::shared_ptr<EnvNode> &Env) {
  for (const auto &Attr : DynamicAttrs) {
    if (!Attr.value())
      continue;
    dfs(Attr.key(), Env);
    dfs(*Attr.value(), Env);
  }
}

std::shared_ptr<EnvNode> VariableLookupAnalysis::dfsAttrs(
    const SemaAttrs &SA, const std::shared_ptr<EnvNode> &Env,
    const Node *Syntax, Definition::DefinitionSource Source) {
  if (SA.isRecursive()) {
    // rec { }, or let ... in ...
    DefBuilder DB;
    // For each static names, create a name binding.
    for (const auto &[Name, Attr] : SA.staticAttrs())
      ToDef.insert_or_assign(&Attr.key(), DB.add(Name, &Attr.key(), Source));

    auto NewEnv = std::make_shared<EnvNode>(Env, DB.finish(), Syntax);

    for (const auto &[_, Attr] : SA.staticAttrs()) {
      if (!Attr.value())
        continue;
      if (Attr.kind() == Attribute::AttributeKind::Plain ||
          Attr.kind() == Attribute::AttributeKind::InheritFrom) {
        dfs(*Attr.value(), NewEnv);
      } else {
        assert(Attr.kind() == Attribute::AttributeKind::Inherit);
        dfs(*Attr.value(), Env);
      }
    }

    dfsDynamicAttrs(SA.dynamicAttrs(), NewEnv);
    return NewEnv;
  }

  // Non-recursive. Dispatch nested node with old Env
  for (const auto &[_, Attr] : SA.staticAttrs()) {
    if (!Attr.value())
      continue;
    dfs(*Attr.value(), Env);
  }

  dfsDynamicAttrs(SA.dynamicAttrs(), Env);
  return Env;
};

void VariableLookupAnalysis::dfs(const ExprAttrs &Attrs,
                                 const std::shared_ptr<EnvNode> &Env) {
  const SemaAttrs &SA = Attrs.sema();
  std::shared_ptr<EnvNode> NewEnv =
      dfsAttrs(SA, Env, &Attrs, Definition::DS_Rec);
  if (NewEnv != Env) {
    assert(Attrs.isRecursive() &&
           "NewEnv must be created for recursive attrset");
    if (!NewEnv->isLive()) {
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_ExtraRecursive,
                                         Attrs.rec()->range());
      D.fix("remove `rec` keyword")
          .edit(TextEdit::mkRemoval(Attrs.rec()->range()));
      D.tag(DiagnosticTag::Faded);
    }
  }
}

void VariableLookupAnalysis::dfs(const ExprLet &Let,
                                 const std::shared_ptr<EnvNode> &Env) {

  // Obtain the env object suitable for "in" expression.
  auto GetLetEnv = [&Env, &Let, this]() -> std::shared_ptr<EnvNode> {
    // This is an empty let ... in ... expr, definitely anti-pattern in
    // nix language. We want to passthrough the env then.
    if (!Let.attrs()) {
      return Env;
    }

    // If there are some attributes actually, create a new env.
    const SemaAttrs &SA = Let.attrs()->sema();
    assert(SA.isRecursive() && "let ... in ... attrset must be recursive");
    return dfsAttrs(SA, Env, &Let, Definition::DS_Let);
  };

  auto LetEnv = GetLetEnv();

  if (Let.expr())
    dfs(*Let.expr(), LetEnv);
  emitEnvLivenessWarning(LetEnv);
}

void VariableLookupAnalysis::trivialDispatch(
    const Node &Root, const std::shared_ptr<EnvNode> &Env) {
  for (const Node *Ch : Root.children()) {
    if (!Ch)
      continue;
    dfs(*Ch, Env);
  }
}

void VariableLookupAnalysis::dfs(const ExprWith &With,
                                 const std::shared_ptr<EnvNode> &Env) {
  auto NewEnv = std::make_shared<EnvNode>(Env, EnvNode::DefMap{}, &With);
  if (!WithDefs.contains(&With)) {
    auto NewDef =
        std::make_shared<Definition>(&With.kwWith(), Definition::DS_With);
    ToDef.insert_or_assign(&With.kwWith(), NewDef);
    WithDefs.insert_or_assign(&With, NewDef);
  }

  if (With.with())
    dfs(*With.with(), Env);

  if (With.expr())
    dfs(*With.expr(), NewEnv);

  if (WithDefs.at(&With)->uses().empty()) {
    Diagnostic &D =
        Diags.emplace_back(Diagnostic::DK_ExtraWith, With.kwWith().range());
    Fix &F = D.fix("remove `with` expression")
                 .edit(TextEdit::mkRemoval(With.kwWith().range()));
    if (With.tokSemi())
      F.edit(TextEdit::mkRemoval(With.tokSemi()->range()));
    if (With.with())
      F.edit(TextEdit::mkRemoval(With.with()->range()));
  }
}

void VariableLookupAnalysis::dfs(const Node &Root,
                                 const std::shared_ptr<EnvNode> &Env) {
  Envs.insert({&Root, Env});
  switch (Root.kind()) {
  case Node::NK_ExprVar: {
    const auto &Var = static_cast<const ExprVar &>(Root);
    lookupVar(Var, Env);
    break;
  }
  case Node::NK_ExprLambda: {
    const auto &Lambda = static_cast<const ExprLambda &>(Root);
    dfs(Lambda, Env);
    break;
  }
  case Node::NK_ExprAttrs: {
    const auto &Attrs = static_cast<const ExprAttrs &>(Root);
    dfs(Attrs, Env);
    break;
  }
  case Node::NK_ExprLet: {
    const auto &Let = static_cast<const ExprLet &>(Root);
    dfs(Let, Env);
    break;
  }
  case Node::NK_ExprWith: {
    const auto &With = static_cast<const ExprWith &>(Root);
    dfs(With, Env);
    break;
  }
  default:
    trivialDispatch(Root, Env);
  }
}

void VariableLookupAnalysis::runOnAST(const Node &Root) {
  // Create a basic env
  DefBuilder DB;
  std::vector<std::string> Builtins{
      "__add",
      "__fetchurl",
      "__isFloat",
      "__seq",
      "break",
      "__addDrvOutputDependencies",
      "__filter",
      "__isFunction",
      "__sort",
      "builtins",
      "__addErrorContext",
      "__filterSource",
      "__isInt",
      "__split",
      "derivation",
      "__all",
      "__findFile",
      "__isList",
      "__splitVersion",
      "derivationStrict",
      "__any",
      "__flakeRefToString",
      "__isPath",
      "__storeDir",
      "dirOf",
      "__appendContext",
      "__floor",
      "__isString",
      "__storePath",
      "false",
      "__attrNames",
      "__foldl'",
      "__langVersion",
      "__stringLength",
      "fetchGit",
      "__attrValues",
      "__fromJSON",
      "__length",
      "__sub",
      "fetchMercurial",
      "__bitAnd",
      "__functionArgs",
      "__lessThan",
      "__substring",
      "fetchTarball",
      "__bitOr",
      "__genList",
      "__listToAttrs",
      "__tail",
      "fetchTree",
      "__bitXor",
      "__genericClosure",
      "__mapAttrs",
      "__toFile",
      "fromTOML",
      "__catAttrs",
      "__getAttr",
      "__match",
      "__toJSON",
      "import",
      "__ceil",
      "__getContext",
      "__mul",
      "__toPath",
      "isNull",
      "__compareVersions",
      "__getEnv",
      "__nixPath",
      "__toXML",
      "map",
      "__concatLists",
      "__getFlake",
      "__nixVersion",
      "__trace",
      "null",
      "__concatMap",
      "__groupBy",
      "__parseDrvName",
      "__traceVerbose",
      "placeholder",
      "__concatStringsSep",
      "__hasAttr",
      "__parseFlakeRef",
      "__tryEval",
      "removeAttrs",
      "__convertHash",
      "__hasContext",
      "__partition",
      "__typeOf",
      "scopedImport",
      "__currentSystem",
      "__hashFile",
      "__path",
      "__unsafeDiscardOutputDependency",
      "throw",
      "__currentTime",
      "__hashString",
      "__pathExists",
      "__unsafeDiscardStringContext",
      "toString",
      "__deepSeq",
      "__head",
      "__readDir",
      "__unsafeGetAttrPos",
      "true",
      "__div",
      "__intersectAttrs",
      "__readFile",
      "__zipAttrsWith",
      "__elem",
      "__isAttrs",
      "__readFileType",
      "abort",
      "__elemAt",
      "__isBool",
      "__replaceStrings",
      "baseNameOf",
      // This is an undocumented keyword actually.
      "__curPos",
  };

  for (const auto &Builtin : Builtins)
    DB.addBuiltin(Builtin);

  auto Env = std::make_shared<EnvNode>(nullptr, DB.finish(), nullptr);

  dfs(Root, Env);
}

VariableLookupAnalysis::VariableLookupAnalysis(std::vector<Diagnostic> &Diags)
    : Diags(Diags) {
  spawnAttrSetEval({}, NixpkgsEval);
  if (NixpkgsEval)
    NixpkgsClient = NixpkgsEval->client();
  if (NixpkgsClient)
    NixpkgsClient->setLoggingEnabled(false);
}

const std::unordered_set<std::string> *
VariableLookupAnalysis::ensureNixpkgsKnownAttrsCached(
    const std::vector<std::string> &Scope) {
  if (!NixpkgsClient)
    return nullptr;

  std::string FullPath = joinScope(Scope);
  if (NixpkgsKnownAttrs.contains(FullPath))
    return &NixpkgsKnownAttrs.at(FullPath);

  if (NixpkgsKnownAttrs.empty()) {
    std::binary_semaphore Ready(0);
    NixpkgsClient->evalExpr(
        "import <nixpkgs> { }",
        [&Ready](llvm::Expected<std::optional<std::string>> Resp) {
          Ready.release();
        });
    Ready.acquire();
  }

  nixd::AttrPathCompleteParams Params;
  Params.Scope = Scope;
  Params.Prefix = "";
  Params.MaxItems = 0;

  std::binary_semaphore Ready(0);
  std::vector<std::string> Names;
  NixpkgsClient->attrpathComplete(
      Params,
      [&Names, &Ready](llvm::Expected<nixd::AttrPathCompleteResponse> Resp) {
        if (Resp)
          Names = *Resp;
        Ready.release();
      });
  Ready.acquire();

  auto &Bucket = NixpkgsKnownAttrs[FullPath];
  Bucket.insert(Names.begin(), Names.end());
  return &Bucket;
}

VariableLookupAnalysis::~VariableLookupAnalysis() = default;

const EnvNode *VariableLookupAnalysis::env(const Node *N) const {
  if (!Envs.contains(N))
    return nullptr;
  return Envs.at(N).get();
}
