# RUN: nixd-lint %s | FileCheck %s

{
  inherit aaa bbb;

  # CHECK: duplicated attr `aaa`
  # CHECK: previously defined here
  inherit "aaa";
}
