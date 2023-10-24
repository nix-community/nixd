# RUN: nixf-ast-dump < %s | FileCheck %s

# CHECK: AttrSet
{
  # CHECK:      Inherit {{.*}}
  # CHECK-NEXT:  Token {{.*}} inherit
  # CHECK-NEXT:  Token 2 a
  # CHECK-NEXT:  Token 1 ;
  inherit a;
  #      CHECK: Inherit {{.*}}
  # CHECK-NEXT:  Token {{.*}} inherit
  # CHECK-NEXT:  Token 2 (
  # CHECK-NEXT:  Token 1 1
  # CHECK-NEXT:  Token 1 )
  # CHECK-NEXT:  Token 2 a
  # CHECK-NEXT:  Token 2 b
  # CHECK-NEXT:  Token 2 c
  # CHECK-NEXT:  Token 1 ;
  inherit (1) a b c;
}
