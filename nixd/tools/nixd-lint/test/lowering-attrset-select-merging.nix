# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

# CHECK: ExprAttrs: { a = { b = 1; c = 2; x = 1; }; }
{
  # CHECK: ExprAttrs: { b = 1; c = 2; x = 1; }
  a.b = 1;
  a.c = 2;

  a = {
    x = 1;
  };
}
