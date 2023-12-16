# RUN: nixf-ast-dump < %s | FileCheck %s

{
  #      CHECK: OpBinary 35
  # CHECK-NEXT:   OpBinary 6
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 2 <
  # CHECK-NEXT:    Token 2 2
  # CHECK-NEXT:   Token 3 ||
  # CHECK-NEXT:   OpBinary 26
  # CHECK-NEXT:    OpBinary 16
  # CHECK-NEXT:     OpBinary 7
  # CHECK-NEXT:      Token 2 2
  # CHECK-NEXT:      Token 3 <=
  # CHECK-NEXT:      Token 2 2
  # CHECK-NEXT:     Token 3 &&
  # CHECK-NEXT:     OpBinary 6
  # CHECK-NEXT:      Token 2 2
  # CHECK-NEXT:      Token 2 >
  # CHECK-NEXT:      Token 2 1
  # CHECK-NEXT:    Token 3 &&
  # CHECK-NEXT:    OpBinary 7
  # CHECK-NEXT:     Token 2 2
  # CHECK-NEXT:     Token 3 >=
  # CHECK-NEXT:     Token 2 2
  a = 1 < 2 || 2 <= 2 && 2 > 1 && 2 >= 2;

  #      CHECK: OpBinary 34
  # CHECK-NEXT: Token 6 false
  # CHECK-NEXT: Token 3 ->
  # CHECK-NEXT: OpBinary 25
  # CHECK-NEXT:  OpBinary 19
  # CHECK-NEXT:   OpNot 6
  # CHECK-NEXT:    Token 1 !
  # CHECK-NEXT:    Token 5 false
  # CHECK-NEXT:   Token 2 &&
  # CHECK-NEXT:   OpBinary 11
  # CHECK-NEXT:    Token 5 false
  # CHECK-NEXT:    Token 2 ==
  # CHECK-NEXT:    Token 4 true
  # CHECK-NEXT:  Token 2 ||
  # CHECK-NEXT:  Token 4 true
  b = false ->!false&&false==true||true;


  #      CHECK: OpBinary 17
  # CHECK-NEXT:    OpBinary 7
  # CHECK-NEXT:     Token 2 1
  # CHECK-NEXT:     Token 3 ==
  # CHECK-NEXT:     Token 2 1
  # CHECK-NEXT:    Token 3 &&
  # CHECK-NEXT:    OpBinary 7
  # CHECK-NEXT:     Token 2 2
  # CHECK-NEXT:     Token 3 !=
  # CHECK-NEXT:     Token 2 3
  eq = 1 == 1 && 2 != 3;

  # CHECK: OpBinary 6
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 2 -
  # CHECK-NEXT:    Token 2 1
  neq = 1 - 1;

  # CHECK: OpBinary 8
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 2 +
  # CHECK-NEXT:    OpNegate 4
  # CHECK-NEXT:     Token 2 -
  # CHECK-NEXT:     Token 2 2
  neq2 = 1 + - 2;
}
