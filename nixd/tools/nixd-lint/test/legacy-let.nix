# RUN: nixd-lint %s | FileCheck %s

let { a = 1; } # CHECK:  using deprecated `let'

; # CHECK: syntax error, unexpected ';', expecting end of file
