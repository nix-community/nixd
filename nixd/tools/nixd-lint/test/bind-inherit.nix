# RUN: nixd-lint -dump-nix-ast %s | FileCheck %s

# CHECK: ExprAttrs: { inherit a ; inherit b ; inherit bar ; inherit c ; inherit foo ; }
{
  # CHECK: ExprVar: a
  inherit a b c bar;

  # CHECK: ExprVar: foo
  inherit "foo";
}
