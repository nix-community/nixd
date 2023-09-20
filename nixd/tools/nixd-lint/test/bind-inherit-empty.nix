# RUN: nixd-lint %s | FileCheck %s

{
  # CHECK: empty inherit expression
  inherit;
}
