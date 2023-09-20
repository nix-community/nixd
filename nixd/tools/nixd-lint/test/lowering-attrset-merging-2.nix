# RUN: nixd-lint %s | FileCheck %s
{
  a.bbb = 1;

  # CHECK: error: duplicated attr `b`
  a.bbb.c = 2;

  # CHECK: duplicated attr `a`
  a = 3;
}
