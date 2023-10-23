#pragma once

#include "RawSyntax.h"

#include <memory>

namespace nixf {

// All language constructs in nix language.
class Syntax {
public:
  enum SyntaxKind {
    // expressions, the can be evaluated to "values"
    SK_BeginExpr,
#define EXPR(NAME) SK_##NAME,
#include "SyntaxKinds.inc"
#undef EXPR
    SK_EndExpr,
  };

private:
  SyntaxKind Kind;
  Syntax *Parent;

public:
  Syntax(SyntaxKind Kind, Syntax *Parent = nullptr)
      : Kind(Kind), Parent(Parent) {}
  SyntaxKind getKind() { return Kind; }

  Syntax *getParent() { return Parent; }
  void setParent(Syntax *NewParent) { Parent = NewParent; }

  /// returns printable name of enum kind.
  static const char *getName(SyntaxKind Kind);

  constexpr const char *getName() { return getName(getKind()); }

  /// Check if this node is an expression
  static bool isExpr(SyntaxKind Kind) {
    return SK_BeginExpr < Kind && Kind < SK_EndExpr;
  }

  bool isExpr() { return isExpr(Kind); }
};

/// Abstract class for all expressions
struct ExprSyntax : Syntax {
  ExprSyntax(SyntaxKind Kind) : Syntax(Kind) {}
};

struct LambdaSyntax : Syntax {
  LambdaSyntax() : Syntax(SK_Lambda) {}
};

} // namespace nixf
