/// \file
/// \brief Semantic Actions while building the AST
///
/// Non grammatical errors (e.g. duplicating) are detected here.

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Basic.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Basic/Range.h"

#include <map>

namespace nixf {

class Sema {
  std::string_view Src;
  std::vector<Diagnostic> &Diags;

public:
  Sema(std::string_view Src, std::vector<Diagnostic> &Diags)
      : Src(Src), Diags(Diags) {}

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

  void dupAttr(std::string Name, LexerCursorRange Range, LexerCursorRange Prev);

  /// \note Name must not be null
  void insertAttr(SemaAttrs &SA, std::shared_ptr<AttrName> Name,
                  std::shared_ptr<Expr> E, bool IsInherit);

  /// Select into \p Attr the attribute specified by \p Path, or create one if
  /// not exists, until reached the inner-most attr. Similar to `mkdir -p`.
  ///
  /// \return The selected or created attribute.
  SemaAttrs *selectOrCreate(SemaAttrs &SA,
                            const std::vector<std::shared_ptr<AttrName>> &Path);

  /// Insert the binding: `AttrPath = E;` into \p Attr
  void addAttr(SemaAttrs &Attr, const AttrPath &Path, std::shared_ptr<Expr> E);

  void lowerInheritName(SemaAttrs &SA, std::shared_ptr<AttrName> Name,
                        std::shared_ptr<Expr> E);

  void lowerInherit(SemaAttrs &Attr, const Inherit &Inherit);

  void lowerBinds(SemaAttrs &SA, const Binds &B);

  std::shared_ptr<ExprAttrs> onExprAttrs(LexerCursorRange Range,
                                         std::shared_ptr<Binds> Binds,
                                         std::shared_ptr<Misc> Rec);
};

} // namespace nixf
