# RUN: nixf-ast-dump < %s | FileCheck %s
{
  #      CHECK: {{Path 8}}
  # CHECK-NEXT:  Token 3 ./
  # CHECK-NEXT:  Token 5 ././.
  a = ./././.;

  #      CHECK: {{Path 6}}
  # CHECK-NEXT:  Token 3 a/
  # CHECK-NEXT:  Token 3 a/a
  b = a/a/a;

  # TODO:
  # c = a//b;

  #      CHECK: {{Path 9}}
  # CHECK-NEXT:  Token 3 a/
  # CHECK-NEXT:  Token 6 a////b
  d = a/a////b;

  #      CHECK:  Call 9
  # CHECK-NEXT:   Path 7
  # CHECK-NEXT:    Token 3 a/
  # CHECK-NEXT:    Token 4 a/a.
  # CHECK-NEXT:   Token 2 b
  stop = a/a/a. b;

  # CHECK:      Path 5
  # CHECK-NEXT:  Token 2 /
  # CHECK-NEXT:  Token 3 a/a
  d = /a/a;
}
