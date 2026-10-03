# RUN: nixd --lit-test < %s | FileCheck %s

Regression tests for #848: preserve comments around values and between bindings.

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

```nix file:///flatten-attrs-comments-3.nix
{
  a = {
    b.c =
      # IMPORTANT COMMENT
      2;
    # AND COMMENT HERE
    d =
      # ANOTHER COMMENT
      {
        # NESTED COMMENT TOO
        e = 1;
      } # WHAT ABOUT HERE
    ; # AND HERE
  };
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-comments-3.nix"
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
CHECK: "id": 3,
CHECK: "newText": "  \n    a.b.c =\n      # IMPORTANT COMMENT\n      2;\n    # AND COMMENT HERE\n    a.d =\n      # ANOTHER COMMENT\n      {\n        # NESTED COMMENT TOO\n        e = 1;\n      } # WHAT ABOUT HERE\n    ; # AND HERE\n  ",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 4,
CHECK-NEXT: "line": 13
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
