/// \file
/// \brief AST nodes.
///
/// Declares the AST nodes used by the parser, the nodes may be used in latter
/// stages, for example semantic analysis.
/// It is expected that they may share some nodes, so they are reference
/// counted.

#pragma once

#include "Nodes/Attrs.h"
#include "Nodes/Basic.h"
#include "Nodes/Expr.h"
#include "Nodes/Lambda.h"
#include "Nodes/Op.h"
#include "Nodes/Simple.h"
