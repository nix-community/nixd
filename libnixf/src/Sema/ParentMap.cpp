#include "nixf/Sema/ParentMap.h"

using namespace nixf;

void ParentMapAnalysis::dfs(const Node *N, const Node *Parent) {
  if (!N)
    return;
  ParentMap.insert({N, Parent});
  for (const Node *Ch : N->children())
    dfs(Ch, N);
}

const Node *ParentMapAnalysis::query(const Node &N) const {
  return ParentMap.contains(&N) ? ParentMap.at(&N) : nullptr;
}

const Node *ParentMapAnalysis::upExpr(const Node &N) const {

  if (Expr::isExpr(N.kind()))
    return &N;
  const Node *Up = query(N);
  if (isRoot(Up, N) || !Up)
    return nullptr;
  return upExpr(*Up);
}

const Node *ParentMapAnalysis::upTo(const Node &N, Node::NodeKind Kind) const {

  if (N.kind() == Kind)
    return &N;
  const Node *Up = query(N);
  if (isRoot(Up, N) || !Up)
    return nullptr;
  return upTo(*Up, Kind);
}

void ParentMapAnalysis::runOnAST(const Node &Root) {
  // Special case. Root node has itself as "parent".
  dfs(&Root, &Root);
}

bool nixf::ParentMapAnalysis::isRoot(const Node *Up, const Node &N) {
  return Up == &N;
}

bool nixf::ParentMapAnalysis::isRoot(const Node &N) const {
  return isRoot(query(N), N);
}
