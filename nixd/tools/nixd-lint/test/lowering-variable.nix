# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

{
  # CHECK: ExprVar: a
  a = a;

  # CHECK: ExprPos: __curPos
  p = __curPos;
}
