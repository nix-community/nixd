# RUN: nixf-ast-dump < %s | FileCheck %s


{
  #      CHECK: With 10
  # CHECK-NEXT:    Token 5 with
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 1 ;
  # CHECK-NEXT:    Token 2 1
  a = with 1; 1;

  #      CHECK: Assert 12
  # CHECK-NEXT:    Token 7 assert
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 1 ;
  # CHECK-NEXT:    Token 2 1
  b = assert 1; 1;
}
