# RUN: nixf-ast-dump < %s | FileCheck %s

# CHECK:      String
# CHECK:      Token 9 asdasdasd
# CHECK-NEXT: Interpolation 13
# CHECK-NEXT:  Token 2 ${
# CHECK-NEXT:  String 9
# CHECK-NEXT:   Token 2  "
# CHECK-NEXT:   Token 6 string
# CHECK-NEXT:   Token 1 "
# CHECK-NEXT:  Token 2  }
# CHECK-NEXT: Token 6 asdasd
# CHECK-NEXT: Token 1 "
"asdasdasd${ "string" }asdasd"
