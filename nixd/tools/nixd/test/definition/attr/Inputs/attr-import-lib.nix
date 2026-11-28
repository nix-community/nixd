{ prefix }:
rec {
  double = x: x * 2;
  helpers = {
    triple = x: x * 3;
  };
  label = "${prefix}-label";
}
