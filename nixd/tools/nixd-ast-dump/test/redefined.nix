# RUN: nixd-ast-dump %s | FileCheck %s
# RUN: valgrind --leak-check=full --error-exitcode=1 nixd-ast-dump %s | FileCheck %s

# CHECK: ExprAttrs
rec {

  x.y.z = 1;
  x.y.z = 2;

  asda.asdasd = 1;
  asda.asdasd.asdasd = 2;

  a = 1;
  a = 2;
  b = 1;
  b = 2;


  nested = {
    bar = {
      x = 1;
    };
  };

  nested.bar.x = 2;

  inherited = {
    inherit nested;
    nested.a = 3;
  };
}
