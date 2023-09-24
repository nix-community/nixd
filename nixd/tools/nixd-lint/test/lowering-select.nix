# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

{
  # CHECK: ExprSelect: (1).a.b.c
  a = 1 . a.b.c;

  # CHECK: ExprSelect: (1).a.b.c or ((2).foo.bar)
  b = 1 . a.b.c or 2 . foo.bar;
}
