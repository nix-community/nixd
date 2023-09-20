# RUN: nixd-lint %s | FileCheck %s
{
  inherit aa;

  # CHECK: duplicated attr `aa`
  aa = 1;
}
