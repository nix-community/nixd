# RUN: nixf-ast-dump < %s | FileCheck %s

# CHECK: AttrSet
{
  # CHECK: Binding
  a = 1;
  # CHECK: Binding
  b = 2;

  # CHECK: Binding {{.*}}
  # CHECK:  AttrPath {{.*}}
  # CHECK:   Token {{.*}} a
  # CHECK:   Token 1 .
  # CHECK:   Token 1 b
  # CHECK:   Token 1 .
  # CHECK:   Token 1 c
  a.b.c = 54;


  #      CHECK:     AttrPath 5
  # CHECK-NEXT:       Token 1 a
  # CHECK-NEXT:       Token 1 .
  # CHECK-NEXT:       Token 1 b
  # CHECK-NEXT:       Token 1 .
  # CHECK-NEXT:       Token 1 c
  # CHECK-NEXT:      Token 1 =
  # CHECK-NEXT:      Token 1 1
  # CHECK-NEXT:      Token 1 ;
    p = {a.b.c=1;};


  # CHECK: AttrSet 27
  # CHECK-NEXT:    Token 2 {
  # CHECK-NEXT:    Binds 21
  # CHECK-NEXT:     Binding 21
  # CHECK-NEXT:      AttrPath 16
  # CHECK-NEXT:       String 11
  # CHECK-NEXT:        Token 6 "
  # CHECK-NEXT:        Interpolation 4
  # CHECK-NEXT:         Token 2 ${
  # CHECK-NEXT:         Token 1 c
  # CHECK-NEXT:         Token 1 }
  # CHECK-NEXT:        Token 1 "
  # CHECK-NEXT:       Token 1 .
  # CHECK-NEXT:       Interpolation 4
  # CHECK-NEXT:        Token 2 ${
  # CHECK-NEXT:        Token 1 d
  # CHECK-NEXT:        Token 1 }
  # CHECK-NEXT:      Token 2 =
  # CHECK-NEXT:      Token 2 3
  # CHECK-NEXT:      Token 1 ;
  # CHECK-NEXT:    Token 4 }
  # CHECK-NEXT:   Token 1 ;
  dynamic = {
    "${c}".${d} = 3;
  };


  # CHECK: AttrSet 3
  # CHECK-NEXT:   Token 2 {
  # CHECK-NEXT:   Binds 0
  # CHECK-NEXT:   Token 1 }
  empty = {};
    }
