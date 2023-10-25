# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s


# CHECK: expected }
# CHECK: fixes: rec {}
rec { 
