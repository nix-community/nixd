# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG


{
  # DIAG: error: expected an expression as attr body
  # DIAG: fixes:   a = expr ;
  a = ;


    # DIAG: error: expected an expression as inherited
    # DIAG: fixes:     inherit ( expr);
    inherit ();

  # DIAG: error: expected an expression as interpolation
  # DIAG: fixes:   b = "${ expr}";
  b = "${}";

  # DIAG: error: expected an expression as lambda body
  # DIAG:   c = a: ;
  # DIAG: fixes:   c = a: expr ;
  c = a: ;
}
