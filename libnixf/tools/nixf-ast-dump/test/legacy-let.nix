# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

{
  # CHECK: LegacyLet 65
  a = let {
    aa = 1;
    bb = 2;
    body = 3;
    inherit inh;
  };

  # DIAG: error: expected }
  # DIAG: fixes:     inherit inh;}
  # DIAG-NEXT: note: to match this {
  b = let {
    ccc = 1;
    ddd = 2;
    body = 3;
    inherit inh;
  ;
}
