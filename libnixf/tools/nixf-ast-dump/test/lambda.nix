# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

# CHECK: AttrSet {{.*}}
{
  # CHECK: Lambda 5
  a = a: 1;

  # CHECK: Lambda 25
  b = a @ { formal, f ? 1 }: 1;

  # CHECK: Lambda 30
  c = a @ { formal, f ? 1, ... }: 1;

  # CHECK:      Lambda 14
  # CHECK-NEXT:  LambdaArg 10
  # CHECK-NEXT:   BracedFormals 10
  # CHECK-NEXT:    Token 2 {
  # CHECK-NEXT:    Formals 6
  # CHECK-NEXT:     Formal 6
  # CHECK-NEXT:      Token 2 a
  # CHECK-NEXT:      Token 2 ?
  # CHECK-NEXT:      Token 2 1
  # CHECK-NEXT:    Token 2 }
  # CHECK-NEXT:  Token 2 :
  # CHECK-NEXT:  Token 2 1
  quest = { a ? 1 } : 1;

  d = { formal } @ a: 1;

  e = { formal }: 1;

  # CHECK:      Lambda 29
  # CHECK-NEXT:  LambdaArg 26
  # CHECK-NEXT:   BracedFormals 26
  # CHECK-NEXT:    Token 2 {
  # CHECK-NEXT:    Formals 22
  # CHECK-NEXT:     Formal 7
  # CHECK-NEXT:      Token 7 formal
  # CHECK-NEXT:     Token 1 ,
  # CHECK-NEXT:     Formal 4
  # CHECK-NEXT:      Token 4 ...
  # CHECK-NEXT:     Token 2 ,
  # CHECK-NEXT:     Formal 8
  # CHECK-NEXT:      Token 8 formal2
  # CHECK-NEXT:    Token 2 }
  # CHECK-NEXT:  Token 1 :
  # CHECK-NEXT:  Token 2 1
  formalElip = { formal, ... , formal2 }: 1;

  # DIAG: error: missing seperator `,` between two lambda formals
  # DIAG: fixes:   f = { a, b, x }: 1;
  f = { a, b x }: 1;

  # DIAG: error: expected an expression
  # DIAG: fixes:   g = { a, b ? expr , ... }: 1;
  g = { a, b ? , ... }: 1;

  # TODO: ugly.
  # h = { a, b, ?, ... }: 1;

  # DIAG: error: expected ?
  # DIAG: fixes:   i = { a, b ?  c.d }: 1;
  i = { a, b c.d }: 1;

  j = {
    # DIAG: error: expected an identifier
    # DIAG: fixes:       {} @arg 1;
    a =
      {} @ 1;
      };

      }
