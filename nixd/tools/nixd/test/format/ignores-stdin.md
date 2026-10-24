# Test that nixd survives a formatter that exits without reading the whole
# document. The document is larger than a pipe buffer, so nixd's write to
# the formatter's stdin fails once the formatter exits. That write must not
# raise SIGPIPE and kill nixd; the formatter's output is still used.

# RUN: %S/make-large-document-test.sh > %t.md
# RUN: timeout 60 nixd --lit-test -config='{ "formatting": { "command": ["%S/nixfmt"] } }' < %t.md | FileCheck %s

```
CHECK: "id": 2,
CHECK: "newText": "Hello\n",
```
