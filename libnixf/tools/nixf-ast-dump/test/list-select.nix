# RUN: nixf-ast-dump < %s | FileCheck %s

# CHECK: Select 15
# CHECK: Select 24
# CHECK: Select 32
# CHECK: Token 3 or
[
  "sss"."ssss"
  "aaaaasd"."asdasdsad"

  "asdasda"."asdasd" or "asda"
]
