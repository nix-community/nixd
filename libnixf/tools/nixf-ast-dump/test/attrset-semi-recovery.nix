# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG


{
  #      DIAG: error: expected ; at the end of binding
  # DIAG-NEXT: a = 1
  # DIAG-NEXT: fixes:   a = 1;
  # DIAG-NEXT: note: attrname declares at here
  a = 1

    #      DIAG: error: expected ; at the end of binding
    # DIAG-NEXT: b = 2
    # DIAG-NEXT: fixes:     b = 2;
    b = 2

  # DIAG: error: expected ; at the end of binding
  # DIAG-NEXT:   multi = a b c d
  # DIAG-NEXT: fixes:   multi = a b c d;
  # DIAG-NEXT: note: attrname declares at here
  # DIAG-NEXT: multi = a b c d
  multi = a b c d

  # DIAG: error: expected ; at the end of binding
  # DIAG-NEXT:   e = 1
  # DIAG-NEXT: fixes:   e = 1;
  # DIAG-NEXT: note: attrname declares at here
  # DIAG-NEXT: e = 1
  e = 1

  y = a b c.d e false;

  #      DIAG: error: expected =
  # DIAG-NEXT:   g 1;
  # DIAG-NEXT: fixes:   g = 1;
  # DIAG-NEXT: note: to match this attrname
  # DIAG-NEXT:   g 1;
  g 1;

  # DIAG-EMPTY:

  a = 1;

  # Recover from previous errors.

  x = 2;

  a = 1;


}
