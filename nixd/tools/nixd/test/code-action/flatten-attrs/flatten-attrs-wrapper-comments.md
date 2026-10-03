# RUN: nixd --lit-test < %s | FileCheck %s

Keep wrapper and dotted-path comments exactly once, with quoted path segments.

```json
{
  "jsonrpc": "2.0",
  "id": 0,
  "method": "initialize",
  "params": {
    "processId": 123,
    "rootPath": "",
    "capabilities": {},
    "trace": "off"
  }
}
```

```nix file:///flatten-attrs-wrapper-comments-2.nix
{
  a /* PATH */ . # DOT
  "b.c" /* BEFORE EQ */ = # BEFORE OPEN
  { # FIRST
    x /* INNER PATH */ = /* VALUE */ 1 /* BEFORE SEMI */; # LAST
    y = 2;
    # BEFORE CLOSE
  } /* AFTER CLOSE */ ; # AFTER OUTER SEMI
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-wrapper-comments-2.nix"
    },
    "range": {
      "start": {
        "line": 1,
        "character": 2
      },
      "end": {
        "line": 1,
        "character": 2
      }
    },
    "context": {
      "diagnostics": [],
      "triggerKind": 2
    }
  }
}
```

```
CHECK: "id": 2,
CHECK: "newText": " /* PATH */  # DOT\n   /* BEFORE EQ */  # BEFORE OPEN\n   # FIRST\n    a.\"b.c\".x /* INNER PATH */ = /* VALUE */ 1 /* BEFORE SEMI */; # LAST\n    a.\"b.c\".y = 2;\n    # BEFORE CLOSE\n   /* AFTER CLOSE */ ",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 23,
CHECK-NEXT: "line": 7
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 2,
CHECK-NEXT: "line": 1
CHECK: "title": "Flatten nested attribute set"
```

```json
{
  "jsonrpc": "2.0",
  "method": "exit"
}
```
