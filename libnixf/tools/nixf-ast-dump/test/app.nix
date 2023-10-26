# RUN: nixf-ast-dump < %s | FileCheck %s

#      CHECK: Call {{.*}}
# CHECK-NEXT:  Token {{.*}} 1
# CHECK-NEXT:  Call 6
# CHECK-NEXT:   Token 2 2
# CHECK-NEXT:   Call 4
# CHECK-NEXT:    Token 2 3
# CHECK-NEXT:    Token 2 4
1 2 3 4
