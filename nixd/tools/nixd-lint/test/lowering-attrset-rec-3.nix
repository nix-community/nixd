# RUN: nixd-lint %s | FileCheck %s
{

  foo = rec {
    x = 1;
    y = 2;
  };

  # CHECK: merging two attributes with different `rec` modifiers, the latter will be implicitly ignored
  # CHECK: this attribute set is recursive
  # CHECK: while this attribute set is marked as non-recursive, it will be considered as recursive
  foo = {
    bar = 1;
  };
}
