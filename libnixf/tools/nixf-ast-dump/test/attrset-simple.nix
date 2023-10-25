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

  a
    }
