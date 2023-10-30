# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

{
  # CHECK: Let 9
  # CHECK-NEXT:    Token 4 let
  # CHECK-NEXT:    Binds 0
  # CHECK-NEXT:    Token 3 in
  # CHECK-NEXT:    Token 2 1
  a = let in 1;

  # CHECK: Let 16
  # CHECK-NEXT:    Token 4 let
  # CHECK-NEXT:    Binds 7
  # CHECK-NEXT:     Binding 7
  # CHECK-NEXT:      AttrPath 2
  # CHECK-NEXT:       Token 2 a
  # CHECK-NEXT:      Token 2 =
  # CHECK-NEXT:      Token 2 1
  # CHECK-NEXT:      Token 1 ;
  # CHECK-NEXT:    Token 3 in
  # CHECK-NEXT:    Token 2 1
  b = let a = 1; in 1;


  # CHECK: Let 14
  # CHECK-NEXT:    Token 4 let
  # CHECK-NEXT:    Binds 8
  # CHECK-NEXT:     Binding 8
  # CHECK-NEXT:      AttrPath 3
  # CHECK-NEXT:       Token 3 a
  # CHECK-NEXT:      Token 2 =
  # CHECK-NEXT:      Token 2 1
  # CHECK-NEXT:      Token 1 ;
  # CHECK-NEXT:    Token 2 1
  # DIAG: fixes:   missing_in = let  a = 1; in  1;
  missing_in = let  a = 1; 1;

  # CHECK: Let 20
  # CHECK-NEXT:    Token 4 let
  # CHECK-NEXT:    Binds 11
  # CHECK-NEXT:     Inherit 11
  # CHECK-NEXT:      Token 8 inherit
  # CHECK-NEXT:      Token 2 a
  # CHECK-NEXT:      Token 1 ;
  # CHECK-NEXT:    Token 3 in
  # CHECK-NEXT:    Token 2 1
  inh = let inherit a; in 1;

  # DIAG: fixes:   c = let a = 1; in 1;
  c = let a = 1; asdasd $$$ in 1;
}
