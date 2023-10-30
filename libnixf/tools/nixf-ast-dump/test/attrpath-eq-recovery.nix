# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG


{
  # DIAG: error: unexpected text between attrpath and `=`
  # DIAG: fixes:   foo  = 1;
  foo  asds sa = 1;
  # DIAG: fixes:   a = ;
  a ;

  # DIAG: fixes: b = pkgs.callPackage ./.;
  b pkgs.callPackage ./.;

  c = {
    a = 1;

    # DIAG: error: unexpected text in binding declaration
    # DIAG: fixes:     };
    c
      };


      # DIAG-EMPTY:
      d = 1;
  }
