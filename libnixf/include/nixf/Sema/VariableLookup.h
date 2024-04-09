/// \file
/// \brief Lookup variable names, from it's parent scope.
///
/// This file declares a variable-lookup analysis on AST.
/// We do variable lookup for liveness checking, and emit diagnostics
/// like "unused with", or "undefined variable".
/// The implementation aims to be consistent with C++ nix (NixOS/nix).

#pragma once

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Basic.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Basic/Nodes/Simple.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nixf {

/// \brief Represents a definition
class Definition {
  std::vector<const ExprVar *> Uses;
  const Node *Syntax;

public:
  explicit Definition(const Node *Syntax) : Syntax(Syntax) {}
  Definition(std::vector<const ExprVar *> Uses, const Node *Syntax)
      : Uses(std::move(Uses)), Syntax(Syntax) {}

  [[nodiscard]] const Node *syntax() const { return Syntax; }

  [[nodiscard]] const std::vector<const ExprVar *> &uses() const {
    return Uses;
  }

  void usedBy(const ExprVar &User) { Uses.emplace_back(&User); }

  [[nodiscard]] bool isBuiltin() const { return !Syntax; }
};

/// \brief A set of variable definitions, which may inherit parent environment.
class EnvNode {
public:
  using DefMap = std::map<std::string, std::shared_ptr<Definition>>;

private:
  const std::shared_ptr<EnvNode> Parent; // Points to the parent node.

  DefMap Defs; // Definitions.

  const Node *Syntax;

public:
  EnvNode(std::shared_ptr<EnvNode> Parent, DefMap Defs, const Node *Syntax)
      : Parent(std::move(Parent)), Defs(std::move(Defs)), Syntax(Syntax) {}

  [[nodiscard]] EnvNode *parent() const { return Parent.get(); }

  /// \brief Where this node comes from.
  [[nodiscard]] const Node *syntax() const { return Syntax; }

  [[nodiscard]] bool isWith() const {
    return Syntax && Syntax->kind() == Node::NK_ExprWith;
  }

  [[nodiscard]] const DefMap &defs() const { return Defs; }

  [[nodiscard]] bool isLive() const;
};

class VariableLookupAnalysis {
public:
  enum class LookupResultKind {
    Undefined,
    FromWith,
    Defined,
  };

  struct LookupResult {
    LookupResultKind Kind;
    std::shared_ptr<const Definition> Def;
  };

private:
  std::vector<Diagnostic> &Diags;

  std::map<const Node *, std::shared_ptr<Definition>>
      WithDefs; // record with ... ; users.

  void lookupVar(const ExprVar &Var, const std::shared_ptr<EnvNode> &Env);

  std::shared_ptr<EnvNode> dfsAttrs(const SemaAttrs &SA,
                                    const std::shared_ptr<EnvNode> &Env,
                                    const Node *Syntax);

  void emitEnvLivenessWarning(const std::shared_ptr<EnvNode> &NewEnv);

  void dfsDynamicAttrs(const std::vector<Attribute> &DynamicAttrs,
                       const std::shared_ptr<EnvNode> &Env);

  // "dfs" is an abbreviation of "Deep-First-Search".
  void dfs(const ExprLambda &Lambda, const std::shared_ptr<EnvNode> &Env);
  void dfs(const ExprAttrs &Attrs, const std::shared_ptr<EnvNode> &Env);
  void dfs(const ExprLet &Let, const std::shared_ptr<EnvNode> &Env);
  void dfs(const ExprWith &With, const std::shared_ptr<EnvNode> &Env);

  void dfs(const Node &Root, const std::shared_ptr<EnvNode> &Env);

  void trivialDispatch(const Node &Root, const std::shared_ptr<EnvNode> &Env);

  std::map<const ExprVar *, LookupResult> Results;

public:
  VariableLookupAnalysis(std::vector<Diagnostic> &Diags);

  void runOnAST(const Node &Root);

  LookupResult query(const ExprVar &Var) { return Results.at(&Var); }
};

} // namespace nixf
