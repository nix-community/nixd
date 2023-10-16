# RUN: nixd-lint %s | FileCheck %s

# CHECK: duplicated function formal declaration
a @ { b
  # CHECK: previously declared here
, a ? 1
  # CHECK: function argument duplicated to a function forma
, a
  # CHECK: previously declared here
}: { }
