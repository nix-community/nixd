# RUN: nixd-ast-dump -bindv -range %s | FileCheck %s
# RUN: valgrind --leak-check=full --error-exitcode=1 nixd-ast-dump %s

# Tests error recovoery for:
# binds
# | binds attrpath error

# CHECK: ExprAttrs: rec { a = 1; b = <nixd:err>; c = 1; d = c; } 9:1 14:2
rec {
  a = 1;
  b; # CHECK: nixd::ExprError: <nixd:err>
  c = 1;
  d = c; # CHECK: ExprVar: c 13:7 13:8 level: 0 displ: 2
}
