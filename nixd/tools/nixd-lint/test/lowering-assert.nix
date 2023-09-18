# RUN: nixd-lint %s | FileCheck %s

# CHECK: function argument duplicated to a function formal
assert 0; a @ { a }: 1
