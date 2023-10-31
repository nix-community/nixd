# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

{
  # CHECK: AttrPath
  # CHECK-NEXT: Token {{.*}} a
  # CHECK-NEXT: Token 1 .
  # CHECK-NEXT: Token 2 or
  # DIAG: warning: keyword `or` used as an identifier
  a.or = 1;

  #      CHECK: Call 9
  # CHECK-NEXT:  Token 2 a
  # CHECK-NEXT:  Call 5
  # CHECK-NEXT:   Token 2 b
  # CHECK-NEXT:   Token 3 or
  # CHECK-NEXT:  Token 2 c
  # DIAG: warning: keyword `or` used as an identifier
  b = a b or c;
}
