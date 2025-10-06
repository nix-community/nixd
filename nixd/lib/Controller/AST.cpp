#include "AST.h"

#include "nixd/Protocol/AttrSet.h"

#include <unordered_set>

using namespace nixf;

namespace {

/// \brief The attrpath contains dynamic attrname.
struct AttrPathHasDynamicError : std::exception {
  [[nodiscard]] const char *what() const noexcept override {
    return "the attrpath has dynamic attribute name";
  }
};

/// \brief Find nested attrpath.
/// e.g.   a.b.c.d
///               ^<-  a.b.c.d
/// { a = 1; b = { c = d; }; }
///                 ^ b.c
///
/// SelectAttrPath := { a.b.c.d = 1; }
///                     ^~~~~~~<--------- such "selection"
/// ValueAttrPath := N is a "value", find how it's nested.
void getSelectAttrPath(const nixf::AttrName &N,
                       const nixf::ParentMapAnalysis &PM,
                       std::vector<std::string> &Path) {
  const nixf::Node *Up = PM.query(N);
  assert(Up && "Naked attrname!");
  assert(Up->kind() == Node::NK_AttrPath &&
         "Invoked in non-attrpath name! (Slipped inherit?)");
  const auto &APath = static_cast<const nixf::AttrPath &>(*Up);
  // Iterate on attr names
  Path.reserve(APath.names().size());
  for (const auto &Name : APath.names()) {
    if (!Name->isStatic())
      throw AttrPathHasDynamicError();
    Path.emplace_back(Name->staticName());
    if (Name.get() == &N)
      break;
  }
}

/// \copydoc getSelectAttrPath
void getValueAttrPath(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                      std::vector<std::string> &Path) {
  if (PM.isRoot(N))
    return;

  const nixf::Node *Up = PM.query(N);
  if (!Up)
    return;

  Up = PM.upExpr(*Up);

  if (!Up)
    return;

  // Only attrs can "nest something"
  if (Up->kind() != Node::NK_ExprAttrs)
    return;

  std::vector<std::string_view> Basic;
  // Recursively search up all nested entries
  if (!PM.isRoot(*Up))
    getValueAttrPath(*Up, PM, Path);

  // Find out how "N" gets nested.
  const auto &UpAttrs = static_cast<const nixf::ExprAttrs &>(*Up);
  assert(UpAttrs.binds() && "empty binds cannot nest anything!");
  for (const std::shared_ptr<nixf::Node> &Attr : UpAttrs.binds()->bindings()) {
    assert(Attr);
    if (Attr->kind() == Node::NK_Inherit) // Cannot deal with inherits
      continue;
    assert(Attr->kind() == Node::NK_Binding);
    const auto &Binding = static_cast<const nixf::Binding &>(*Attr);
    if (Binding.value().get() == &N) {
      getSelectAttrPath(*Binding.path().names().back(), PM, Path);
      return;
    }
  }
  assert(false && "must have corresbonding value");
  __builtin_unreachable();
}

void getNestedAttrPath(const nixf::AttrName &N,
                       const nixf::ParentMapAnalysis &PM,
                       std::vector<std::string> &Path) {
  if (const auto *Expr = PM.upExpr(N))
    getValueAttrPath(*Expr, PM, Path);
  getSelectAttrPath(N, PM, Path);
}

} // namespace

[[nodiscard]] const EnvNode *nixd::upEnv(const nixf::Node &Desc,
                                         const VariableLookupAnalysis &VLA,
                                         const ParentMapAnalysis &PM) {
  const nixf::Node *N = &Desc; // @nonnull
  while (!VLA.env(N) && PM.query(*N) && !PM.isRoot(*N))
    N = PM.query(*N);
  assert(N);
  return VLA.env(N);
}

bool nixd::havePackageScope(const Node &N, const VariableLookupAnalysis &VLA,
                            const ParentMapAnalysis &PM) {
  // Firstly find the first "env" enclosed with this variable.
  const EnvNode *Env = upEnv(N, VLA, PM);
  if (!Env)
    return false;

  // Then, search up until there are some `with`.
  for (; Env; Env = Env->parent()) {
    if (!Env->isWith())
      continue;
    // this env is "with" expression.
    const Node *With = Env->syntax();
    assert(With && With->kind() == Node::NK_ExprWith);
    const Node *WithBody = static_cast<const ExprWith &>(*With).with();
    if (!WithBody)
      continue; // skip incomplete with epxression.

    // Se if it is "ExprVar“. Stupid.
    if (WithBody->kind() != Node::NK_ExprVar)
      continue;

    // Hardcoded "pkgs", even more stupid.
    if (static_cast<const ExprVar &>(*WithBody).id().name() == idioms::Pkgs)
      return true;
  }
  return false;
}

// Idioms.
namespace {

using IdiomSetT = std::unordered_set<std::string_view>;

IdiomSetT IdiomSet{nixd::idioms::Pkgs, nixd::idioms::Lib};

auto ItLib = IdiomSet.find(nixd::idioms::Lib);
auto ItPkgs = IdiomSet.find(nixd::idioms::Pkgs);

nixd::Selector getKnownIdiomSelector(IdiomSetT::iterator It) {
  // Unknown name, cannot deal with it.
  if (It == IdiomSet.end())
    throw nixd::idioms::NotAnIdiomException();

  return [&]() -> nixd::Selector {
    if (It == ItLib) {
      return {std::string(nixd::idioms::Lib)};
    }
    if (It == ItPkgs) {
      return {};
    }
    assert(false && "Unhandled idiom iterator?");
    __builtin_unreachable();
    return {};
  }();
}

nixd::Selector varSelector(const nixf::ExprVar &Var) {
  return getKnownIdiomSelector(IdiomSet.find(Var.id().name()));
};

nixd::Selector withSelector(const nixf::ExprWith &With,
                            const nixf::VariableLookupAnalysis &VLA,
                            const nixf::ParentMapAnalysis &PM) {
  if (!With.with())
    throw nixd::idioms::NotAnIdiomException();
  switch (With.with()->kind()) {
  case Node::NK_ExprVar:
    return nixd::idioms::mkVarSelector(
        static_cast<const nixf::ExprVar &>(*With.with()), VLA, PM);
  case Node::NK_ExprSelect:
    return nixd::idioms::mkSelector(
        static_cast<const nixf::ExprSelect &>(*With.with()), VLA, PM);
  default:
    break;
  }
  throw nixd::idioms::NotAnIdiomException();
}

} // namespace

nixd::Selector nixd::idioms::mkSelector(const nixf::ExprSelect &Select,
                                        nixd::Selector BaseSelector) {
  if (Select.path())
    return nixd::idioms::mkSelector(
        *static_cast<const nixf::AttrPath *>(Select.path()),
        std::move(BaseSelector));
  return BaseSelector;
}

nixd::Selector
nixd::idioms::mkVarSelector(const nixf::ExprVar &Var,
                            const nixf::VariableLookupAnalysis &VLA,
                            const nixf::ParentMapAnalysis &PM) {
  // Only check if the variable can be recogonized by some idiom.

  using ResultKind = VariableLookupAnalysis::LookupResultKind;
  auto Result = VLA.query(Var);
  switch (Result.Kind) {
  case ResultKind::Undefined:
  case ResultKind::Defined:
    return varSelector(Var);
  case ResultKind::FromWith: {
    assert(Result.Def && "FromWith variables should contains definition");
    const nixf::Definition &Def = *Result.Def;
    if (!Def.syntax())
      throw NotAnIdiomException();

    // The syntax
    //
    //    with pkgs; with lib; [ ]
    //
    // does provide both "pkgs" + "lib" scopes.
    //
    // However, in current implementation we will only consider nested "with".
    // That is, only "lib" variables will be considered.
    const nixf::Node &Syntax = *Def.syntax();
    const nixf::Node *With = PM.query(Syntax);
    assert(With && "parent of kwWith should be the with expression");
    assert(With->kind() == nixf::Node::NK_ExprWith);
    Selector WithSelector =
        withSelector(static_cast<const nixf::ExprWith &>(*With), VLA, PM);

    // Append variable name after "with" expression selector.
    // e.g.
    //
    //      with pkgs; [ fo ]
    //                   ^
    // The result will be {pkgs, fo}
    WithSelector.emplace_back(Var.id().name());

    return WithSelector;
  }
  case ResultKind::NoSuchVar:
    throw NoSuchVarException();
  case ResultKind::PrimOp:
    throw NotAnIdiomException();
  }
  assert(false && "switch fallthrough!");
  __builtin_unreachable();
  return {};
}

nixd::Selector nixd::idioms::mkSelector(const nixf::AttrPath &AP,
                                        Selector BaseSelector) {
  const auto &Names = AP.names();
  for (const auto &Name : Names) {
    if (!Name->isStatic())
      throw DynamicNameException();
    BaseSelector.emplace_back(Name->staticName());
  }
  return BaseSelector;
}

nixd::Selector nixd::idioms::mkSelector(const nixf::ExprSelect &Sel,
                                        const nixf::VariableLookupAnalysis &VLA,
                                        const nixf::ParentMapAnalysis &PM) {
  if (Sel.expr().kind() != Node::NK_ExprVar)
    throw NotVariableSelect();

  const auto &Var = static_cast<ExprVar &>(Sel.expr());

  auto BaseSelector = mkVarSelector(Var, VLA, PM);

  return mkSelector(Sel, std::move(BaseSelector));
}

std::pair<std::vector<std::string>, std::string>
nixd::getScopeAndPrefix(const Node &N, const ParentMapAnalysis &PM) {
  if (N.kind() != Node::NK_Identifier)
    return {};

  // FIXME: impl scoped packages
  std::string Prefix = static_cast<const Identifier &>(N).name();
  return {{}, Prefix};
}

nixd::FindAttrPathResult nixd::findAttrPath(const nixf::Node &N,
                                            const nixf::ParentMapAnalysis &PM,
                                            std::vector<std::string> &Path) {
  using R = nixd::FindAttrPathResult;
  // If this is in "inherit", don't consider it is an attrpath.
  if (PM.upTo(N, Node::NK_Inherit))
    return R::Inherit;

  if (const Node *Name = PM.upTo(N, Node::NK_AttrName)) {
    try {
      getNestedAttrPath(static_cast<const AttrName &>(*Name), PM, Path);
    } catch (AttrPathHasDynamicError &E) {
      return R::WithDynamic;
    }
    return R::OK;
  }

  // Consider this is an "extra" dot.
  if (const Node *DotNode = PM.upTo(N, Node::NK_Dot)) {
    const auto &D = static_cast<const Dot &>(*DotNode);

    if (D.prev().kind() != Node::NK_AttrName)
      return R::NotAttrPath;

    try {
      getNestedAttrPath(static_cast<const AttrName &>(D.prev()), PM, Path);
      Path.emplace_back("");
      return R::OK;
    } catch (AttrPathHasDynamicError &E) {
      return R::WithDynamic;
    }
  }

  return R::NotAttrPath;
}

nixd::FindAttrPathResult
nixd::findAttrPathForOptions(const nixf::Node &N,
                             const nixf::ParentMapAnalysis &PM,
                             std::vector<std::string> &Path) {

  const auto R = findAttrPath(N, PM, Path);
  if (R == FindAttrPathResult::OK) {
    // FIXME: Only do this in NixOS module files and in flakes only in module
    // functions passed to nixpkgs.lib.nixosSystem.
    if (!Path.empty() && Path[0] == "config") {
      Path.erase(Path.begin());
    }
  }
  return R;
}
