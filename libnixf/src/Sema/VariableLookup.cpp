#include "nixf/Sema/VariableLookup.h"
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Sema/PrimOpInfo.h"

#include <set>

using namespace nixf;

namespace {

std::set<std::string> Constants{
    "true",           "false",           "null",
    "__currentTime",  "__currentSystem", "__nixVersion",
    "__storeDir",     "__langVersion",   "__importNative",
    "__traceVerbose", "__nixPath",       "derivation",
};

/// Builder a map of definitions. If there are something overlapped, maybe issue
/// a diagnostic.
class DefBuilder {
  EnvNode::DefMap Def;
  std::vector<Diagnostic> &Diags;

  std::shared_ptr<Definition> addSimple(std::string Name, const Node *Entry,
                                        Definition::DefinitionSource Source) {
    assert(!Def.contains(Name));
    auto NewDef = std::make_shared<Definition>(Entry, Source);
    Def.insert({std::move(Name), NewDef});
    return NewDef;
  }

public:
  DefBuilder(std::vector<Diagnostic> &Diags) : Diags(Diags) {}

  void addBuiltin(std::string Name) {
    // Don't need to record def map for builtins.
    auto _ = addSimple(std::move(Name), nullptr, Definition::DS_Builtin);
  }

  [[nodiscard("Record ToDef Map!")]] std::shared_ptr<Definition>
  add(std::string Name, const Node *Entry, Definition::DefinitionSource Source,
      bool IsInheritFromBuiltin) {
    auto PrimOpLookup = lookupGlobalPrimOpInfo(Name);
    if (PrimOpLookup == PrimopLookupResult::Found && !IsInheritFromBuiltin) {
      // Overriding a builtin primop is discouraged.
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_PrimOpOverridden, Entry->range());
      D << Name;
    }

    // Lookup constants
    if (Constants.contains(Name)) {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_ConstantOverridden, Entry->range());
      D << Name;
    }

    return addSimple(std::move(Name), Entry, Source);
  }

  EnvNode::DefMap finish() { return std::move(Def); }
};

/// Special check for inherited from builtins attribute.
/// e.g. inherit (builtins) foo bar;
/// Returns true if the attribute is inherited from builtins.
///
/// Suppress warnings for overriding primops in this case.
bool checkInheritedFromBuiltin(const Attribute &Attr) {
  if (Attr.kind() != Attribute::AttributeKind::InheritFrom)
    return false;

  assert(Attr.value() &&
         "select expr desugared from inherit should not be null");
  assert(Attr.value()->kind() == Node::NK_ExprSelect &&
         "desugared inherited from should be a select expr");
  const auto &Select = static_cast<const ExprSelect &>(*Attr.value());
  if (Select.expr().kind() == Node::NK_ExprVar) {
    const auto &Var = static_cast<const ExprVar &>(Select.expr());
    return Var.id().name() == "builtins";
  }
  return false;
}

bool isBuiltinConstant(const std::string &Name) {
  if (Name.starts_with("_"))
    return false;
  return Constants.contains(Name) || Constants.contains("__" + Name);
}

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
    // We cannot determine where it precisely come from, thus mark all Envs
    // alive.
    if (CurEnv->isWith()) {
      WithEnvs.emplace_back(CurEnv);
    }
  }

  if (Def) {
    Def->usedBy(Var);
    Results.insert({&Var, LookupResult{LookupResultKind::Defined, Def}});
  } else if (!WithEnvs.empty()) { // comes from enclosed "with" expressions.
    // Collect all `with` expressions that could provide this variable's
    // binding. This is stored for later queries (e.g., by code actions that
    // need to determine if converting a `with` to `let/inherit` is safe).
    std::vector<const ExprWith *> WithScopes;
    for (const auto *WithEnv : WithEnvs) {
      Def = WithDefs.at(WithEnv->syntax());
      Def->usedBy(Var);
      WithScopes.push_back(static_cast<const ExprWith *>(WithEnv->syntax()));
    }
    VarWithScopes.insert({&Var, std::move(WithScopes)});
    Results.insert({&Var, LookupResult{LookupResultKind::FromWith, Def}});
  } else {
    // Check if this is a primop.
    switch (lookupGlobalPrimOpInfo(Name)) {
    case PrimopLookupResult::Found:
      assert(false && "primop name should be defined");
      break;
    case PrimopLookupResult::PrefixedFound: {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_PrimOpNeedsPrefix, Var.range());
      D.fix("use `builtins.` prefix")
          .edit(TextEdit::mkInsertion(Var.range().lCur(), "builtins."));
      Results.insert(
          {&Var, LookupResult{LookupResultKind::Undefined, nullptr}});
      break;
    }
    case PrimopLookupResult::NotFound:
      // Otherwise, this variable is undefined.
      Results.insert(
          {&Var, LookupResult{LookupResultKind::Undefined, nullptr}});
      Diagnostic &Diag =
          Diags.emplace_back(Diagnostic::DK_UndefinedVariable, Var.range());
      Diag << Var.id().name();
      break;
    }
  }
}

void VariableLookupAnalysis::dfs(const ExprLambda &Lambda,
                                 const std::shared_ptr<EnvNode> &Env) {
  // Early exit for in-complete lambda.
  if (!Lambda.body())
    return;

  // Create a new EnvNode, as lambdas may have formal & arg.
  DefBuilder DBuilder(Diags);
  assert(Lambda.arg());
  const LambdaArg &Arg = *Lambda.arg();

  // foo: body
  // ^~~<------- add function argument.
  if (Arg.id()) {
    if (!Arg.formals()) {
      ToDef.insert_or_assign(Arg.id(),
                             DBuilder.add(Arg.id()->name(), Arg.id(),
                                          Definition::DS_LambdaArg,
                                          /*IsInheritFromBuiltin=*/false));
      // Function arg cannot duplicate to it's formal.
      // If it this unluckily happens, we would like to skip this definition.
    } else if (!Arg.formals()->dedup().contains(Arg.id()->name())) {
      ToDef.insert_or_assign(Arg.id(),
                             DBuilder.add(Arg.id()->name(), Arg.id(),
                                          Definition::DS_LambdaWithArg_Arg,
                                          /*IsInheritFromBuiltin=*/false));
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
                             DBuilder.add(Name, Formal->id(), Source,
                                          /*IsInheritFromBuiltin=*/false));
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
  // Check inherit (builtins) for diagnostics
  checkInheritBuiltins(SA, Syntax);

  if (SA.isRecursive()) {
    // rec { }, or let ... in ...
    DefBuilder DB(Diags);
    // For each static names, create a name binding.
    for (const auto &[Name, Attr] : SA.staticAttrs()) {
      ToDef.insert_or_assign(
          &Attr.key(),
          DB.add(Name, &Attr.key(), Source, checkInheritedFromBuiltin(Attr)));
    }

    auto NewEnv = std::make_shared<EnvNode>(Env, DB.finish(), Syntax);

    for (const auto &[_, Attr] : SA.staticAttrs()) {
      if (!Attr.value())
        continue;
      // Skip traversing InheritFrom values to avoid duplicate diagnostics
      if (Attr.kind() == Attribute::AttributeKind::InheritFrom &&
          checkInheritedFromBuiltin(Attr))
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
    // Skip traversing InheritFrom values to avoid duplicate diagnostics
    if (Attr.kind() == Attribute::AttributeKind::InheritFrom &&
        checkInheritedFromBuiltin(Attr))
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
    // nix language. Create a trivial env and return.
    if (!Let.attrs()) {
      auto NewEnv = std::make_shared<EnvNode>(Env, EnvNode::DefMap{}, &Let);
      return NewEnv;
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

void VariableLookupAnalysis::checkBuiltins(const ExprSelect &Sel) {
  if (!Sel.path())
    return;

  if (Sel.expr().kind() != Node::NK_ExprVar)
    return;

  const auto &Builtins = static_cast<const ExprVar &>(Sel.expr());
  if (Builtins.id().name() != "builtins")
    return;

  const auto &AP = *Sel.path();

  if (AP.names().size() != 1)
    return;

  AttrName &First = *AP.names()[0];
  if (!First.isStatic())
    return;

  const auto &Name = First.staticName();

  switch (lookupGlobalPrimOpInfo(Name)) {
  case PrimopLookupResult::Found: {
    Diagnostic &D = Diags.emplace_back(Diagnostic::DK_PrimOpRemovablePrefix,
                                       Builtins.range());
    Fix &F =
        D.fix("remove `builtins.` prefix")
            .edit(TextEdit::mkRemoval(Builtins.range())); // remove `builtins`

    if (Sel.dot()) {
      // remove the dot also.
      F.edit(TextEdit::mkRemoval(Sel.dot()->range()));
    }
    return;
  }
  case PrimopLookupResult::PrefixedFound:
    return;
  case PrimopLookupResult::NotFound:
    if (!isBuiltinConstant(Name)) {
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_PrimOpUnknown,
                                         AP.names()[0]->range());
      D << Name;
      return;
    }
  }
}

void VariableLookupAnalysis::checkInheritBuiltins(
    const SemaAttrs &SA, const Node *Syntax) {
  for (const auto &[Name, Attr] : SA.staticAttrs()) {
    if (Attr.kind() != Attribute::AttributeKind::InheritFrom)
      continue;

    if (!checkInheritedFromBuiltin(Attr))
      continue;

    // Only emit diagnostic for let blocks, not for attribute sets
    // to avoid changing the structure of attribute sets
    if (Syntax->kind() != Node::NK_ExprLet)
      continue;

    // Check if the inherited name is a prelude builtin
    if (lookupGlobalPrimOpInfo(Name) == PrimopLookupResult::Found) {
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_PrimOpRemovablePrefix,
                                         Attr.key().range());
      D.fix("remove unnecessary inherit")
          .edit(TextEdit::mkRemoval(Attr.key().range()));
    }
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
  case Node::NK_ExprSelect: {
    trivialDispatch(Root, Env);
    const auto &Sel = static_cast<const ExprSelect &>(Root);
    checkBuiltins(Sel);
    break;
  }
  default:
    trivialDispatch(Root, Env);
  }
}

void VariableLookupAnalysis::runOnAST(const Node &Root) {
  // Create a basic env
  DefBuilder DB(Diags);

  for (const auto &[Name, Info] : PrimOpsInfo) {
    if (!Info.Internal) {
      // Only add non-internal primops without "__" prefix.
      DB.addBuiltin(Name);
    }
  }

  for (const auto &Builtin : Constants)
    DB.addBuiltin(Builtin);

  DB.addBuiltin("builtins");
  // This is an undocumented keyword actually.
  DB.addBuiltin(std::string("__curPos"));

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
