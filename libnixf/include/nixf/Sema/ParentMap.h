/// \file
/// \brief ParentMap analysis.
///
/// This is used to construct upward edges. For each node, record it's direct
/// parent. (Abstract Syntax TREE only have one parent for each node).

#pragma once

#include "nixf/Basic/Nodes/Basic.h"

#include <map>

namespace nixf {

class ParentMapAnalysis {
  std::map<const Node *, const Node *> ParentMap;

  void dfs(const Node *N, const Node *Parent);

public:
  void runOnAST(const Node &Root);

  [[nodiscard]] const Node *query(const Node &N) const;

  static bool isRoot(const Node *Up, const Node &N);

  /// \brief Search up until the node becomes a concrete expression.
  /// a
  /// ^<-----   ID -> ExprVar
  [[nodiscard]] const Node *upExpr(const Node &N) const;

  /// \brief Search up until some kind of node is found.
  [[nodiscard]] const Node *upTo(const Node &N, Node::NodeKind Kind) const;

  [[nodiscard]] bool isRoot(const Node &N) const;
};

} // namespace nixf
