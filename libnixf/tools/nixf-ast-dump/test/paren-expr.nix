# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

{
    #      CHECK: Paren 6
    # CHECK-NEXT:   Token 2 (
    # CHECK-NEXT:   Token 2 3
    # CHECK-NEXT:   Token 2 )
    a = ( 3 );

    # DIAG: error: expected )
    # DIAG: fixes:     b = ( 42) ;
    # DIAG: note: to match this (
    b = ( 42 ;
}
