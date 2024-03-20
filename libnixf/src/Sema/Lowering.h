/// \file
/// \brief Semantic lowering of AST nodes.
///
/// Syntax AST nodes are lowered to semantic AST nodes. They share part of the
/// nodes actually, for example `ExprInt`.
#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Sema/Lowering.h"

namespace nixf {

class Lowering {
  std::vector<Diagnostic> &Diags;
  std::string_view Src;
  std::map<Node *, Node *> &LoweringMap;

public:
  Lowering(std::vector<Diagnostic> &Diags, std::string_view Src,
           std::map<Node *, Node *> &LoweringMap)
      : Diags(Diags), Src(Src), LoweringMap(LoweringMap) {}

  void dupAttr(std::string Name, LexerCursorRange Range, LexerCursorRange Prev);

  /// \note Name must not be null
  void insertAttr(ExprSemaAttrs &SA, std::shared_ptr<AttrName> Name,
                  std::shared_ptr<Expr> E, bool IsInherit);

  /// Select into \p Attr the attribute specified by \p Path, or create one if
  /// not exists, until reached the inner-most attr. Similar to `mkdir -p`.
  ///
  /// \return The selected or created attribute.
  ExprSemaAttrs *
  selectOrCreate(ExprSemaAttrs &SA,
                 const std::vector<std::shared_ptr<AttrName>> &Path);

  /// Insert the binding: `AttrPath = E;` into \p Attr
  void addAttr(ExprSemaAttrs &Attr, const AttrPath &Path,
               std::shared_ptr<Expr> E);

  /// \brief Perform lowering.
  std::shared_ptr<Node> lower(std::shared_ptr<Node> AST);

  using FormalVector = Formals::FormalVector;

  /// \brief Make text edits to remove a formal
  static void removeFormal(Fix &F, const FormalVector::const_iterator &Rm,
                           const FormalVector &FV);

  /// \brief Check if there is a seperator "," between formals
  void checkFormalSep(const FormalVector &FV);

  /// \brief Check if ellipsis "..."
  void checkFormalEllipsis(const FormalVector &FV);

  /// \brief Diagnose empty formal i.e. single comma
  //
  //  e.g. `{ , } : 1`
  void checkFormalEmpty(const FormalVector &FV);

  /// \brief Deduplicate formals.
  void dedupFormal(std::map<std::string, const Formal *> &Dedup,
                   const FormalVector &FV);

  /// FIXME: this should be rewritten as immutable nodes.
  void lowerFormals(Formals &FS);

  /// \brief Desugar inherit (expr) a, inherit a, into select, or variable.
  static std::shared_ptr<Expr>
  desugarInheritExpr(std::shared_ptr<AttrName> Name, std::shared_ptr<Expr> E);

  void lowerInheritName(ExprSemaAttrs &SA, std::shared_ptr<AttrName> Name,
                        std::shared_ptr<Expr> E);

  void lowerInherit(ExprSemaAttrs &Attr, const Inherit &Inherit);

  void lowerBinds(ExprSemaAttrs &SA, const Binds &B);

  std::shared_ptr<ExprSemaAttrs> lowerExprAttrs(const ExprAttrs &Attrs);
};

} // namespace nixf
