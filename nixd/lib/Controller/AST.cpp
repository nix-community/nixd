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

  // FIXME: scoped packages is not yet implemented.
  std::string Prefix = static_cast<const Identifier &>(N).name();
  return {{}, Prefix};
}
