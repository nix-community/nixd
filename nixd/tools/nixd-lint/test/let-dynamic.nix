# RUN: nixd-lint %s | FileCheck %s

# CHECK: dynamic attributes are not allowed in let ... in ... expression
let
  ${1} = 3;
  a = 1;
in
1
