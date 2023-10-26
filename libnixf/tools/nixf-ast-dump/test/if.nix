# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG


{
  #      CHECK: If 19
  # CHECK-NEXT:    Token 3 if
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 5 then
  # CHECK-NEXT:    Token 2 1
  # CHECK-NEXT:    Token 5 else
  # CHECK-NEXT:    Token 2 1
  a = if 1 then 1 else 1;

  #      DIAG: error: expected an expression as `if` body
  # DIAG-NEXT:   b = if then else;
  # DIAG-NEXT: fixes:   b = if expr then else;
  # DIAG-NEXT: error: expected an expression as `then` body
  # DIAG-NEXT:   b = if then else;
  # DIAG-NEXT: fixes:   b = if then expr else;
  # DIAG-NEXT: error: expected an expression as `else` body
  # DIAG-NEXT:   b = if then else;
  # DIAG-NEXT: fixes:   b = if then else expr;
  b = if then else;


  #      DIAG: error: expected an expression as `if` body
  # DIAG-NEXT:   c = if;
  # DIAG-NEXT: fixes:   c = if expr;
  # DIAG-NEXT: error: expected `then`
  # DIAG-NEXT:   c = if;
  # DIAG-NEXT: fixes:   c = if then ;
  # DIAG-NEXT: error: expected `else`
  # DIAG-NEXT:   c = if;
  # DIAG-NEXT: fixes:   c = if else ;
  c = if;

  #      DIAG: error: expected an expression as `if` body
  # DIAG-NEXT:   d = if then 1 else 1;
  # DIAG-NEXT: fixes:   d = if expr then 1 else 1;
  d = if then 1 else 1;
}

