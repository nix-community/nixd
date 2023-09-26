# RUN: nixd-lint %s | FileCheck %s
# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s --check-prefix=AST

# AST: (rec { a = 1; }).body
let { a = 1; } # CHECK:  using deprecated `let'

; # CHECK: syntax error, unexpected ';', expecting end of file
