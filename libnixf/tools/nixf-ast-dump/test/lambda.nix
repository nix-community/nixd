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

  d = { formal } @ a: 1;

  e = { formal }: 1;

  # DIAG: error: missing seperator `,` between two lambda formals
  # DIAG: fixes:   f = { a, b, x }: 1;
  # DIAG: note: first formal declares at here
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
