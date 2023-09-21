# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

# CHECK: ExprLet: (let x = 1; y = 2; z = 3; in 1)
let
  x = 1;
  y = 2;
  z = 3;
in
1
