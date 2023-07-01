# RUN: nixd-ast-dump -range %s | FileCheck %s

# CHECK: ExprAttrs: { b = 2; x = 1; } 4:1 7:2
{
  x = 1;
  b = 2;
}
