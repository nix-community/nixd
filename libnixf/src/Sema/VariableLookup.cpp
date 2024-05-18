#include "nixf/Sema/VariableLookup.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Lambda.h"

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
    if (Def->uses().empty()) {
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_DefinitionNotUsed,
                                         Def->syntax()->range());
      D << Name;
      D.tag(DiagnosticTag::Faded);
    }
  }
}

void VariableLookupAnalysis::lookupVar(const ExprVar &Var,
                                       const std::shared_ptr<EnvNode> &Env) {
  const std::string &Name = Var.id().name();

  bool EnclosedWith = false; // If there is a "With" enclosed this var name.
  EnvNode *WithEnv = nullptr;
  EnvNode *CurEnv = Env.get();
  std::shared_ptr<Definition> Def;
  for (; CurEnv; CurEnv = CurEnv->parent()) {
    if (CurEnv->defs().contains(Name)) {
      Def = CurEnv->defs().at(Name);
      break;
    }
    // Find the most nested `with` expression, and set uses.
    if (CurEnv->isWith() && !EnclosedWith) {
      EnclosedWith = true;
      WithEnv = CurEnv;
    }
  }

  if (Def) {
    Def->usedBy(Var);
    Results.insert({&Var, LookupResult{LookupResultKind::Defined, Def}});

    if (EnclosedWith) {
      // Escaping from "with" to outer scope.
      // https://github.com/NixOS/nix/issues/490

      assert(WithEnv && "EnclosedWith -> WithEnv");
      // Make a diagnostic.
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_EscapingWith, Var.range());
      if (Def->syntax()) {
        D.note(Note::NK_VarBindToThis, Def->syntax()->range());
      }
      const auto &KwWith =
          static_cast<const nixf::ExprWith *>(WithEnv->syntax())->kwWith();
      D.note(Note::NK_EscapingWith, KwWith.range());
    }
    return;
  }
  if (EnclosedWith) {
    Def = WithDefs.at(WithEnv->syntax());
    Def->usedBy(Var);
    Results.insert({&Var, LookupResult{LookupResultKind::FromWith, Def}});
    return;
  }

  // Otherwise, this variable is undefined.
  Results.insert({&Var, LookupResult{LookupResultKind::Undefined, nullptr}});
  Diagnostic &Diag =
      Diags.emplace_back(Diagnostic::DK_UndefinedVariable, Var.range());
  Diag << Var.id().name();
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
    // Function arg cannot duplicate to it's formal.
    // If it this unluckily happens, we would like to skip this definition.
    if (!Arg.formals() || !Arg.formals()->dedup().contains(Arg.id()->name()))
      ToDef.insert_or_assign(Arg.id(), DBuilder.add(Arg.id()->name(), Arg.id(),
                                                    Definition::DS_LambdaArg));
  }

  // { foo, bar, ... } : body
  ///  ^~~~~~~~~<--------------  add function formals.
  if (Arg.formals())
    for (const auto &[Name, Formal] : Arg.formals()->dedup())
      ToDef.insert_or_assign(
          Formal->id(),
          DBuilder.add(Name, Formal->id(), Definition::DS_LambdaFormal));

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
      dfs(*Attr.value(), NewEnv);
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
  };

  for (const auto &Builtin : Builtins)
    DB.addBuiltin(Builtin);

  auto Env = std::make_shared<EnvNode>(nullptr, DB.finish(), nullptr);

  dfs(Root, Env);
}

VariableLookupAnalysis::VariableLookupAnalysis(std::vector<Diagnostic> &Diags)
    : Diags(Diags) {}

const EnvNode *VariableLookupAnalysis::env(const Node *N) const {
  if (!Envs.contains(N))
    return nullptr;
  return Envs.at(N).get();
}
