# RUN: nixd-ast-dump -bindv %s | FileCheck %s

rec {
  a = {
    x = 1;
    b = 2;
  };

  foo = rec {
    # CHECK: ExprVar: a  level: 1 displ: 0
    y = a;
    bar = (
      let
        x = 1;
        y = 2;
      in
      {
        # CHECK: ExprVar: x  level: 0 displ: 0
        z = x;
      }
    );
  };

  lambda =
    { x
    , y
    , z
    }: {
      # CHECK: ExprVar: x  level: 0 displ: 0
      vx = x;
      # CHECK:  ExprVar: y  level: 0 displ: 1
      vy = y;
      # CHECK: ExprVar: z  level: 0 displ: 2
      vz = z;
      zz = rec {
        # CHECK: ExprVar: foo  level: 2 displ: 1
        bar = foo;
        # CHECK: ExprVar: z  level: 1 displ: 2
        baz = z;
      };
    };
}
