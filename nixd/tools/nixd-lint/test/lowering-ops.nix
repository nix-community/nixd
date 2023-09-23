# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s
# RUN: nixd-lint -dump-nix-ast -show-position %s | FileCheck --check-prefix=POSITION %s

{
  # POSITION: ExprCall: (__sub 0 1) 6:14
  Position = -1;

  # CHECK: ExprOpNot: (! 1)
  Not = !1;

  # CHECK: ExprCall: (__sub 0 1)
  Negate = -1;

  # CHECK: ExprOpEq: (1 == 1)
  Eq = 1 == 1;

  # CHECK: ExprOpNEq: (1 != 1)
  NEq = 1 != 1;

  # CHECK: ExprCall: (__lessThan 2 1)
  Ge = 1 > 2;

  # CHECK: ExprCall: (__lessThan 1 2)
  Le = 1 < 2;

  # CHECK: (! (__lessThan 2 1))
  Leq = 1 <= 2;

  # CHECK: ExprOpNot: (! (__lessThan 1 2))
  Geq = 1 >= 2;

  # CHECK:  ExprOpAnd: (1 && 2)
  And = 1 && 2;

  # CHECK: ExprOpOr: (1 || 2)
  Or = 1 || 2;

  # CHECK: ExprOpImpl: (1 -> 2)
  Impl = 1 -> 2;

  # CHECK: ExprOpUpdate: (1 // 2)
  Update = 1 // 2;

  # CHECK: ExprConcatStrings: (1 + 2)
  Add = 1 + 2;

  # CHECK: ExprCall: (__sub 1 2)
  Sub = 1 - 2;

  # CHECK: ExprCall: (__mul 1 2)
  Mul = 1 * 2;

  # CHECK: ExprCall: (__div 1 2)
  Div = 1 / 2;

  # CHECK: ExprOpConcatLists: (1 ++ 2)
  Concat = 1 ++ 2;

  # TODO: check that we can deal with dynamic attrpath
  # CHECK: ExprOpHasAttr: ((1) ? a.b.c)
  HasAttr = 1 ? a.b.c;
}
