# RUN: nixd-lint %s | FileCheck %s
rec {
  foo.bar = 1;

  # CHECK: merging two attributes with different `rec` modifiers, the latter will be implicitly ignored

  # CHECK: this attribute set is non-recursive
  # CHECK: while this attribute set is marked as recursive, it will be considered as non-recursive
  foo = rec {
    x = 1;
    y = 2;
  };
}
