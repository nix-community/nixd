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

  std::shared_ptr<Formals> onFormals(LexerCursorRange Range, FormalVector FV);

  /// \brief Desugar inherit (expr) a, inherit a, into select, or variable.
  static std::pair<std::shared_ptr<Expr>, Attribute::AttributeKind>
  desugarInheritExpr(std::shared_ptr<AttrName> Name, std::shared_ptr<Expr> E);

  void dupAttr(std::string Name, LexerCursorRange Range, LexerCursorRange Prev);

  /// \brief Check if these two attrsets has the same "recursive" modifier.
  ///
  /// Official nix implementation implicitly discards the second modifier, this
  /// is somehow error-prone, let's detect it.
  void checkAttrRecursiveForMerge(const ExprAttrs &XAttrs,
                                  const ExprAttrs &YAttrs);

  /// \brief Perform attrsets merging while duplicated fields are both attrsets.
  ///
  /// e.g.
  /// \code{nix}
  /// {
  ///   a = { x = 1; };
  ///   a = { y = 1; };
  /// }
  /// \endcode
  /// We may want to merge both "a = " attrsets into a single one, instead of
  /// report duplicating attrs.
  void mergeAttrSets(SemaAttrs &XAttrs, const SemaAttrs &YAttrs);

  /// \note Name must not be null
  void insertAttr(SemaAttrs &SA, std::shared_ptr<AttrName> Name,
                  std::shared_ptr<Expr> E, Attribute::AttributeKind Kind);

  /// Select into \p Attr the attribute specified by \p Path, or create one if
  /// not exists, until reached the inner-most attr. Similar to `mkdir -p`.
  ///
  /// \return The selected or created attribute.
  SemaAttrs *selectOrCreate(SemaAttrs &SA,
                            const std::vector<std::shared_ptr<AttrName>> &Path);

  /// Insert the binding: `AttrPath = E;` into \p Attr
  void addAttr(SemaAttrs &Attr, const AttrPath &Path, std::shared_ptr<Expr> E);

  void lowerInheritName(SemaAttrs &SA, std::shared_ptr<AttrName> Name,
                        std::shared_ptr<Expr> E,
                        Attribute::AttributeKind InheritKind);

  void lowerInherit(SemaAttrs &Attr, const Inherit &Inherit);

  void lowerBinds(SemaAttrs &SA, const Binds &B);

  std::shared_ptr<ExprAttrs> onExprAttrs(LexerCursorRange Range,
                                         std::shared_ptr<Binds> Binds,
                                         std::shared_ptr<Misc> Rec);

  std::shared_ptr<LambdaArg> onLambdaArg(LexerCursorRange Range,
                                         std::shared_ptr<Identifier> ID,
                                         std::shared_ptr<Formals> F);
};

} // namespace nixf
