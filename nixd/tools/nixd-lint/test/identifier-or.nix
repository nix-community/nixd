# RUN: nixd-lint %s | FileCheck %s

# CHECK: keyword `or` used as an identifier
map or [  ]
