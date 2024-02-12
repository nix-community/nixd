/// \file
/// \brief Semantic lowering of AST nodes.
///
/// This file declares *in place* lowering of AST nodes.
///
/// **Syntax** AST nodes are lowered to **semantic** AST nodes.
/// They share part of the nodes actually, for example `ExprInt`.
///
/// The lowering is done in place, so the AST nodes are mutated.

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes.h"

namespace nixf {

/// \brief Perform semantic lowering on the AST.
void lower(Node *AST, std::string_view Src, std::vector<Diagnostic> &Diags);

} // namespace nixf
