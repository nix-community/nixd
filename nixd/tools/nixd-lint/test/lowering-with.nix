# RUN: nixd-lint %s | FileCheck %s

# CHECK: function argument duplicated to a function formal
with 1; { a } @ a : 1
