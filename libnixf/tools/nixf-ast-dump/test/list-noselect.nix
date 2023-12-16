# RUN: nixf-ast-dump < %s | FileCheck %s

# CHECK: {{List .*}}
# CHECK-NEXT: Token {{.*}} [
# CHECK-NEXT: ListBody 19
[ 1 2 3 4 b/b/b ././ ]
