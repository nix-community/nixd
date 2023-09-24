# RUN: nixd-lint -dump-nix-ast  %s | FileCheck %s

# CHECK: ExprCall: (2 1)
2 1
