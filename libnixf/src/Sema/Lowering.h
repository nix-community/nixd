/// \file
/// \brief Semantic lowering of AST nodes.
///
/// Syntax AST nodes are lowered to semantic AST nodes. They share part of the
/// nodes actually, for example `ExprInt`.

#include <utility>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Sema/Lowering.h"

namespace nixf {

class Lowering {
  std::vector<Diagnostic> &Diags;
  std::string_view Src;

public:
  Lowering(std::vector<Diagnostic> &Diags, std::string_view Src)
      : Diags(Diags), Src(Src) {}

  void dupAttr(std::string Name, LexerCursorRange Range, LexerCursorRange Prev);

  void insertAttr(SemaAttrs &Attr, AttrName &Name,
                  UniqueOrRaw<Evaluable, SemaAttrs> E);

  /// Select into \p Attr the attribute specified by \p Path, or create one if
  /// not exists, until reached the inner-most attr. Similar to `mkdir -p`.
  ///
  /// \return The selected or created attribute.
  SemaAttrs *selectOrCreate(SemaAttrs &Attr,
                            const std::vector<std::unique_ptr<AttrName>> &Path);

  /// Insert the binding: `AttrPath = E;` into \p Attr
  void addAttr(SemaAttrs &Attr, const AttrPath &Path,
               UniqueOrRaw<Evaluable, SemaAttrs> E);

  /// \brief Perform lowering.
  void lower(Node *AST);

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

  void lowerFormals(Formals &FS);

  void lowerInheritName(SemaAttrs &Attr, AttrName *Name, Expr *E);

  void lowerInherit(SemaAttrs &Attr, const Inherit &Inherit);

  void lowerBinds(Binds &B, SemaAttrs &SA);

  void lowerExprAttrs(ExprAttrs &Attrs);
};

} // namespace nixf
