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

```nix file:///flatten-attrs-comments-2.nix
{
  a = {
    b =
      # IMPORTANT COMMENT
      2;
  };
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "textDocument/codeAction",
  "params": {
    "textDocument": {
      "uri": "file:///flatten-attrs-comments-2.nix"
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
CHECK: "newText": "  \n    a.b =\n      # IMPORTANT COMMENT\n      2;\n  ",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 4,
CHECK-NEXT: "line": 5
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
