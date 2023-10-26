# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

# CHECK: {{List .*}}
# CHECK-NEXT: Token {{.*}} [
# CHECK-NEXT: ListBody 19
# DIAG: error: expected ]
# DIAG: fixes: [ 1 2 3 4 b/b/b ././]
#      DIAG: note: to match this [
# DIAG-NEXT: [ 1 2 3 4 b/b/b ././
[ 1 2 3 4 b/b/b ././
