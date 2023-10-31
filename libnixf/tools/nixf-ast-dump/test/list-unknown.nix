# RUN: nixf-ast-dump < %s | FileCheck %s
# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s --check-prefix=DIAG

#      CHECK: List {{.*}}
# CHECK-NEXT:   Token {{.*}} [
# CHECK-NEXT:   ListBody 0
# CHECK-NEXT:  Unknown {{.*}}
# CHECK-NEXT:   Token {{.*}} ?
# CHECK-NEXT:  Unknown 2
# CHECK-NEXT:   Token {{.*}} ]

# TODO: this is not perfect.
# DIAG: fixes: []
[
  ?
]
