# RUN: nixf-ast-dump < %s 2>&1 | FileCheck %s

# CHECK: error: unterminated /* comment
# CHECK: fixes: /**/

/*
