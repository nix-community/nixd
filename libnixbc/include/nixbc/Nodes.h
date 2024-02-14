#pragma once

#include <cstdint>

namespace nixbc {

enum ExprKind : int32_t {
  EK_Assert,
  EK_Attrs,
  EK_Call,
  EK_ConcatStrings,
  EK_Float,
  EK_If,
  EK_Int,
  EK_Lambda,
  EK_Let,
  EK_List,
  EK_OpAnd,
  EK_OpConcatLists,
  EK_OpEq,
  EK_OpHasAttr,
  EK_OpImpl,
  EK_OpNEq,
  EK_OpNot,
  EK_OpOr,
  EK_OpUpdate,
  EK_Path,
  EK_Pos,
  EK_Select,
  EK_String,
  EK_Var,
  EK_With,
};

} // namespace nixbc
