# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

{
  a = 1;
  # DIAG: error: unexpected text in binding declaration
  x.a
