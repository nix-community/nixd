#include "nixf/Bytecode/Write.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Basic/Nodes/Op.h"
#include "nixf/Basic/Nodes/Simple.h"
#include "nixf/Basic/TokenKinds.h"

#include <bc/Write.h>

#include <nixbc/Nodes.h>

using namespace nixf;

using namespace nixbc;

using bc::writeBytecode;

namespace {

NodeHeader mkHeader(ExprKind Kind, const Node &N) {
  uint32_t BeginLine = N.range().lCur().line();
  uint32_t BeginColumn = N.range().lCur().column();

  return {
      Kind,
      reinterpret_cast<std::uintptr_t>(&N),
      BeginLine,
      BeginColumn,
  };
}

// If N is nullptr, turn this into nix variable "null".
// This won't crash the evaluator even if it didn't consider nullptrs.
void writeNull(std::ostream &OS, const Node *N) {
  if (!N) {
    writeBytecode(OS, NodeHeader{EK_Var});
    writeBytecode(OS, std::string("null"));
    return;
  }
  writeBytecode(OS, N);
}

void writeExprInt(std::ostream &OS, const ExprInt &N) {
  writeBytecode(OS, mkHeader(EK_Int, N));
  writeBytecode(OS, N.value());
}

void writeExprFloat(std::ostream &OS, const ExprFloat &Float) {
  writeBytecode(OS, mkHeader(EK_Float, Float));
  writeBytecode(OS, Float.value());
}

void writeInterpolableParts(std::ostream &OS,
                            const std::vector<InterpolablePart> &Fragments,
                            bool ForceString, const Node &N) {
  writeBytecode(OS, mkHeader(EK_ConcatStrings, N));
  writeBytecode(OS, ForceString);

  // Scan the fragments to see how many "Strings" we need to write.
  // Because interpolations might be incomplete, we'd like to omit them.
  // "foo${ }"
  //       ^   <--- skip this empty fragments at all.
  std::size_t Count = 0;
  for (const InterpolablePart &Frag : Fragments) {
    switch (Frag.kind()) {
    case InterpolablePart::SPK_Escaped:
      Count++;
      break;
    case InterpolablePart::SPK_Interpolation:
      if (Frag.interpolation().expr())
        Count++;
      break;
    }
  }

  writeBytecode(OS, Count);

  for (const InterpolablePart &Frag : Fragments) {
    switch (Frag.kind()) {
    case InterpolablePart::SPK_Escaped:
      // FIXME: the location is not correct, not the fragment range.
      writeBytecode(OS, mkHeader(EK_String, N));
      writeBytecode(OS, Frag.escaped());
      break;
    case InterpolablePart::SPK_Interpolation: {
      const Node *Expr = Frag.interpolation().expr();
      if (!Expr)
        continue;
      writeBytecode(OS, Expr);
      break;
    }
    }
  }
}

void writeExprString(std::ostream &OS, const ExprString &String) {
  if (String.isLiteral()) {
    // For literal string, just use "ExprString", it is more efficient to eval.
    writeBytecode(OS, mkHeader(EK_String, String));
    writeBytecode(OS, String.literal());
  } else {
    // Otherwise, the string must be concatenated.
    writeInterpolableParts(OS, String.parts().fragments(), /*ForceString=*/true,
                           String);
  }
}

void writeExprPath(std::ostream &OS, const ExprPath &Path) {
  if (Path.parts().isLiteral()) {
    // For literal path, just use "ExprPath", it is more efficient to eval.
    writeBytecode(OS, mkHeader(EK_Path, Path));
    writeBytecode(OS, Path.parts().literal());
  } else {
    writeInterpolableParts(OS, Path.parts().fragments(), false, Path);
  }
}

void writeExprVar(std::ostream &OS, const ExprVar &Var) {
  writeBytecode(OS, mkHeader(EK_Var, Var));
  writeBytecode(OS, Var.id().name());
}

void writeAttrPath(std::ostream &OS, const AttrPath *AttrPath) {
  if (!AttrPath) {
    writeBytecode(OS, std::size_t(0));
    return;
  }

  writeBytecode(OS, std::size_t(AttrPath->names().size()));

  for (const auto &Name : AttrPath->names()) {
    assert(Name && "AttrName should not be null");
    switch (Name->kind()) {
    case AttrName::ANK_ID:
      writeBytecode(OS, mkHeader(EK_String, *Name));
      writeBytecode(OS, Name->id()->name());
      break;
    case AttrName::ANK_String:
      writeExprString(OS, Name->string());
      break;
    case AttrName::ANK_Interpolation:
      writeBytecode(OS, Name->interpolation().expr());
      break;
    }
  }
}

void writeExprSelect(std::ostream &OS, const ExprSelect &Select) {
  writeBytecode(OS, mkHeader(EK_Select, Select));
  writeBytecode(OS, &Select.expr());
  writeAttrPath(OS, Select.path());
  writeBytecode(OS, Select.defaultExpr());
}

void writeExprOpHasAttr(std::ostream &OS, const ExprOpHasAttr &OpHasAttr) {
  writeBytecode(OS, mkHeader(EK_OpHasAttr, OpHasAttr));
  writeBytecode(OS, OpHasAttr.expr());
  writeAttrPath(OS, OpHasAttr.attrpath());
}

void writePos(std::ostream &OS, const Position &Pos) {
  writeBytecode(OS, uint32_t(Pos.line()));
  writeBytecode(OS, uint32_t(Pos.column()));
}

void writeExprAttrs(std::ostream &OS, const ExprAttrs &Attrs) {
  writeBytecode(OS, mkHeader(EK_Attrs, Attrs));
  writeBytecode(OS, Attrs.isRecursive());

  const SemaAttrs &SA = Attrs.sema();

  std::size_t StaticCount = 0;
  for (const auto &[Key, Attribute] : SA.staticAttrs()) {
    if (Attribute.value())
      StaticCount++;
  }
  writeBytecode(OS, StaticCount);

  for (const auto &[Key, Attribute] : SA.staticAttrs()) {
    if (!Attribute.value())
      continue;
    writeBytecode(OS, Key);

    // AttrDef
    writeBytecode(OS, Attribute.value());
    writePos(OS, Attribute.key().lCur().position());
    writeBytecode(OS, Attribute.fromInherit());
  }

  std::size_t DynamicCount = 0;
  for (const auto &Attribute : SA.dynamicAttrs()) {
    if (Attribute.value())
      DynamicCount++;
  }

  writeBytecode(OS, DynamicCount);

  for (const auto &Attribute : SA.dynamicAttrs()) {
    if (!Attribute.value())
      continue;
    assert(Attribute.key().kind() == Node::NK_AttrName);
    const auto &Name = static_cast<const AttrName &>(Attribute.key());
    switch (Name.kind()) {
    case AttrName::ANK_ID:
      assert(false && "dynamic attrname should not be ID!");
      __builtin_unreachable();
    case AttrName::ANK_String:
      writeExprString(OS, Name.string());
      break;
    case AttrName::ANK_Interpolation:
      writeBytecode(OS, Name.interpolation().expr());
      break;
    }

    writeNull(OS, Attribute.value());
    writePos(OS, Attribute.key().lCur().position());
  }
}

void writeExprList(std::ostream &OS, const ExprList &List) {
  writeBytecode(OS, mkHeader(EK_List, List));

  writeBytecode(OS, std::size_t(List.elements().size()));

  for (const auto &Elm : List.elements())
    writeBytecode(OS, Elm.get());
}

void writeLambdaArg(std::ostream &OS, const LambdaArg *Arg) {
  if (!Arg) {
    writeBytecode(OS, false); // HasArg
    writeBytecode(OS, false); // HasFormals
    return;
  }

  writeBytecode(OS, bool(Arg->id())); // HasArg
  if (Arg->id())
    writeBytecode(OS, Arg->id()->name());

  writeBytecode(OS, bool(Arg->formals())); // HasFormals
  if (Arg->formals()) {
    bool HasEllipsis = false;
    for (const auto &Formal : Arg->formals()->members()) {
      if (Formal->isEllipsis())
        HasEllipsis = true;
    }
    writeBytecode(OS, HasEllipsis);
    writeBytecode(OS, std::size_t(Arg->formals()->dedup().size()));
    for (const auto &[Name, Formal] : Arg->formals()->dedup()) {
      writePos(OS, Formal->lCur().position());
      writeBytecode(OS, Name);
      writeBytecode(OS, Formal->defaultExpr());
    }
  }
}

void writeExprLambda(std::ostream &OS, const ExprLambda &Lambda) {
  writeBytecode(OS, mkHeader(EK_Lambda, Lambda));
  writeLambdaArg(OS, Lambda.arg());
  writeBytecode(OS, Lambda.body());
}

void writeExprCall(std::ostream &OS, const ExprCall &Call) {
  writeBytecode(OS, mkHeader(EK_Call, Call));
  writeBytecode(OS, &Call.fn());

  writeBytecode(OS, std::size_t(Call.args().size()));
  for (const auto &Arg : Call.args())
    writeBytecode(OS, Arg.get());
}

void writeExprLet(std::ostream &OS, const ExprLet &Let) {
  writeBytecode(OS, mkHeader(EK_Let, Let));
  writeBytecode(OS, Let.attrs());
  writeBytecode(OS, Let.expr());
}

void writeExprWith(std::ostream &OS, const ExprWith &With) {
  writeBytecode(OS, mkHeader(EK_With, With));
  writeNull(OS, With.with());
  writeNull(OS, With.expr());
}

void writeExprIf(std::ostream &OS, const ExprIf &If) {
  writeBytecode(OS, mkHeader(EK_If, If));
  writeNull(OS, If.cond());
  writeNull(OS, If.then());
  writeNull(OS, If.elseExpr());
}

void writeExprAssert(std::ostream &OS, const ExprAssert &Assert) {
  writeBytecode(OS, mkHeader(EK_Assert, Assert));
  writeNull(OS, Assert.cond());
  writeNull(OS, Assert.value());
}

// emit a (Call *fn ...
void writeBinOpCall(std::ostream &OS, const Node &Origin, std::string_view Fn) {
  writeBytecode(OS, mkHeader(EK_Call, Origin));
  writeBytecode(OS, mkHeader(EK_Var, Origin));
  writeBytecode(OS, Fn);
  writeBytecode(OS, std::size_t(2));
}

void writeNegate(std::ostream &OS, const Node &Origin, const Node *RHS) {
  // -a -> (Call __sub 0 a)
  writeBinOpCall(OS, Origin, "__sub");
  writeBytecode(OS, mkHeader(EK_Int, Origin));
  writeBytecode(OS, NixInt(0));
  writeNull(OS, RHS);
}

void writeExprUnaryOp(std::ostream &OS, const ExprUnaryOp &Op) {
  switch (Op.op().op()) {
  case tok::tok_op_not:
    writeBytecode(OS, mkHeader(EK_OpNot, Op));
    writeNull(OS, Op.expr());
    break;
  case tok::tok_op_negate:
    writeNegate(OS, Op, Op.expr());
    break;
  default:
    assert(false && "unexpected unary op!");
  }
}

void writeExprBinOp(std::ostream &OS, const ExprBinOp &N) {
  using namespace tok;

  if (!N.lhs() || !N.rhs()) // Skip invalid nodes
    return;

  const Expr *LHS = N.lhs();
  const Expr *RHS = N.rhs();

  switch (N.op().op()) {
  case tok_op_eq:
    writeBytecode(OS, mkHeader(EK_OpEq, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_neq:
    writeBytecode(OS, mkHeader(EK_OpNEq, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_and:
    writeBytecode(OS, mkHeader(EK_OpAnd, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_or:
    writeBytecode(OS, mkHeader(EK_OpOr, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_impl:
    writeBytecode(OS, mkHeader(EK_OpImpl, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_update:
    writeBytecode(OS, mkHeader(EK_OpUpdate, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_concat:
    writeBytecode(OS, mkHeader(EK_OpConcatLists, N));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_div:
    writeBinOpCall(OS, N, "__div");
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_mul:
    writeBinOpCall(OS, N, "__mul");
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_lt:
    // (lt a b)
    writeBinOpCall(OS, N, "__lessThan");
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_le:
    // (le a b) -> (not (lt b a))
    writeBytecode(OS, mkHeader(EK_OpNot, N));
    writeBinOpCall(OS, N, "__lessThan");
    writeNull(OS, RHS);
    writeNull(OS, LHS);
    break;
  case tok_op_gt:
    // (gt a b) -> (lt b a)
    writeBinOpCall(OS, N, "__lessThan");
    writeNull(OS, RHS);
    writeNull(OS, LHS);
    break;
  case tok_op_ge:
    // (ge a b) -> (not (lt a b))
    writeBytecode(OS, mkHeader(EK_OpNot, N));
    writeBinOpCall(OS, N, "__lessThan");
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  case tok_op_add:
    writeBytecode(OS, mkHeader(nixbc::EK_ConcatStrings, N));
    writeBytecode(OS, false); // forceString
    writeBytecode(OS, std::size_t(2));
    writeNull(OS, LHS);
    writeNull(OS, RHS);
    break;
  default:
    assert(false && "Unhandled binary operator");
    __builtin_unreachable();
  }
}

} // namespace

void nixf::writeBytecode(std::ostream &OS, const Node *N) {
  if (!N) {
    bc::writeBytecode(OS, NodeHeader{EK_Null});
    return;
  }
  switch (N->kind()) {
  case Node::NK_ExprInt:
    writeExprInt(OS, static_cast<const ExprInt &>(*N));
    break;
  case Node::NK_ExprFloat:
    writeExprFloat(OS, static_cast<const ExprFloat &>(*N));
    break;
  case Node::NK_ExprString:
    writeExprString(OS, static_cast<const ExprString &>(*N));
    break;
  case Node::NK_ExprPath:
    writeExprPath(OS, static_cast<const ExprPath &>(*N));
    break;
  case Node::NK_ExprVar:
    writeExprVar(OS, static_cast<const ExprVar &>(*N));
    break;
  case Node::NK_ExprSelect:
    writeExprSelect(OS, static_cast<const ExprSelect &>(*N));
    break;
  case Node::NK_ExprOpHasAttr:
    writeExprOpHasAttr(OS, static_cast<const ExprOpHasAttr &>(*N));
    break;
  case Node::NK_ExprAttrs:
    writeExprAttrs(OS, static_cast<const ExprAttrs &>(*N));
    break;
  case Node::NK_ExprList:
    writeExprList(OS, static_cast<const ExprList &>(*N));
    break;
  case Node::NK_ExprLambda:
    writeExprLambda(OS, static_cast<const ExprLambda &>(*N));
    break;
  case Node::NK_ExprCall:
    writeExprCall(OS, static_cast<const ExprCall &>(*N));
    break;
  case Node::NK_ExprLet:
    writeExprLet(OS, static_cast<const ExprLet &>(*N));
    break;
  case Node::NK_ExprWith:
    writeExprWith(OS, static_cast<const ExprWith &>(*N));
    break;
  case Node::NK_ExprIf:
    writeExprIf(OS, static_cast<const ExprIf &>(*N));
    break;
  case Node::NK_ExprAssert:
    writeExprAssert(OS, static_cast<const ExprAssert &>(*N));
    break;
  case Node::NK_ExprUnaryOp:
    writeExprUnaryOp(OS, static_cast<const ExprUnaryOp &>(*N));
    break;
  case Node::NK_ExprBinOp:
    writeExprBinOp(OS, static_cast<const ExprBinOp &>(*N));
    break;
  case Node::NK_ExprParen:
    writeBytecode(OS, static_cast<const ExprParen *>(N)->expr());
    break;
  default:
    assert(false && "unknown node!");
    break;
  }
}
