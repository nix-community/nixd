# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

{
  # DIAG: error: expected an expression as attr body
  # DIAG: fixes:   a = expr ;
  a = ;
    }
