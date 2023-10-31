# RUN: nixf-ast-dump < %s | FileCheck %s

#      CHECK: String
# CHECK-NEXT:   Token {{.*}} "
# CHECK-NEXT:   StringParts 34
# CHECK-NEXT:    Token 9 asdasdasd
# CHECK-NEXT:    Interpolation 13
# CHECK-NEXT:     Token 2 ${
# CHECK-NEXT:     String 9
# CHECK-NEXT:      Token 2 "
# CHECK-NEXT:      StringParts 6
# CHECK-NEXT:       Token 6 string
# CHECK-NEXT:      Token 1 "
# CHECK-NEXT:     Token 2 }
# CHECK-NEXT:    Token 7 asdasd
# CHECK-NEXT:    Token 2 \n
# CHECK-NEXT:    Token 1
# CHECK-NEXT:    Token 2 \\
# CHECK-NEXT:   Token 1 "
"asdasdasd${ "string" }asdasd \n \\"
