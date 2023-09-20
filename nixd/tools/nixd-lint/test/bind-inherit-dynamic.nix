# RUN: nixd-lint %s | FileCheck %s

{
  # CHECK: error: dynamic attributes are not allowed in inherit
  inherit "foo${a}bar";
}
