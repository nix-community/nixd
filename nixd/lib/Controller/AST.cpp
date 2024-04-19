#include "AST.h"

using namespace nixf;

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
    if (static_cast<const ExprVar &>(*WithBody).id().name() == "pkgs")
      return true;
  }
  return false;
}

std::pair<std::vector<std::string>, std::string>
nixd::getScopeAndPrefix(const Node &N, const ParentMapAnalysis &PM) {
  if (N.kind() != Node::NK_Identifer)
    return {};

  // FIXME: impl scoped packages
  std::string Prefix = static_cast<const Identifier &>(N).name();
  return {{}, Prefix};
}

std::vector<std::string_view>
nixd::getValueAttrPath(const nixf::Node &N, const nixf::ParentMapAnalysis &PM) {
  if (PM.isRoot(N))
    return {};

  const nixf::Node *Up = PM.query(N);
  if (!Up)
    return {};

  Up = PM.upExpr(*Up);

  if (!Up)
    return {};

  // Only attrs can "nest something"
  if (Up->kind() != Node::NK_ExprAttrs)
    return {};

  std::vector<std::string_view> Basic;
  // Recursively search up all nested entries
  if (!PM.isRoot(*Up))
    Basic = getValueAttrPath(*Up, PM);

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
      auto Next = getSelectAttrPath(*Binding.path().names().back(), PM);
      Basic.insert(Basic.end(), Next.begin(), Next.end());
      return Basic;
    }
  }
  assert(false && "must have corresbonding value");
  __builtin_unreachable();
  return {};
}

/// \copydoc getValueAttrPath
std::vector<std::string_view>
nixd::getSelectAttrPath(const nixf::AttrName &N,
                        const nixf::ParentMapAnalysis &PM) {
  const nixf::Node *Up = PM.query(N);
  assert(Up);
  switch (Up->kind()) {
  case Node::NK_AttrPath: {
    const auto &APath = static_cast<const nixf::AttrPath &>(*Up);
    // Iterate on attr names
    std::vector<std::string_view> Result;
    Result.reserve(APath.names().size());
    for (const auto &Name : APath.names()) {
      if (!Name->isStatic())
        continue;
      Result.emplace_back(Name->staticName());
      if (Name.get() == &N)
        break;
    }
    return Result;
  }
  case Node::NK_Inherit:
    // FIXME: support Inherit.
    return {};
  default:
    assert(false && "attrname should have either ExprAttrs or Inherit parent!");
    __builtin_unreachable();
  }
}

std::optional<std::vector<std::string_view>>
nixd::findAttrPath(const nixf::Node &N, const nixf::ParentMapAnalysis &PM) {
  if (const Node *Name = PM.upTo(N, Node::NK_AttrName)) {
    std::vector<std::string_view> AttrPath;
    if (const auto *Expr = PM.upExpr(N))
      AttrPath = getValueAttrPath(*Expr, PM);
    auto Select = getSelectAttrPath(static_cast<const AttrName &>(*Name), PM);
    AttrPath.insert(AttrPath.end(), Select.begin(), Select.end());
    // Select "AttrPath" might come from "inherit", thus it can be empty.
    if (AttrPath.empty())
      return std::nullopt;
    return AttrPath;
  }

  // Consider this is an "extra" dot.
  if (const Node *APNode = PM.upTo(N, Node::NK_AttrPath)) {
    const auto &AP = static_cast<const AttrPath &>(*APNode);
    std::vector<std::string_view> AttrPathVec;
    if (const auto *Expr = PM.upExpr(N))
      AttrPathVec = getValueAttrPath(*Expr, PM);
    assert(!AP.names().empty());
    auto Select = getSelectAttrPath(*AP.names().back(), PM);
    AttrPathVec.insert(AttrPathVec.end(), Select.begin(), Select.end());
    AttrPathVec.emplace_back("");
    return AttrPathVec;
  }
  return std::nullopt;
}
