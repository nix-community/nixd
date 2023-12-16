# RUN: nixf-ast-dump < %s | FileCheck %s

{
  # CHECK: Call 6
  # CHECK-NEXT:     Token 2 a
  # CHECK-NEXT:     Token 2 1
  # CHECK-NEXT:     Token 2 2
  # CHECK-NEXT:    Token 2 +
  # CHECK-NEXT:    Token 2 3
  a = a 1 2 + 3;

}
