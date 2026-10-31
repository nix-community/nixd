# RUN: nixd --lit-test < %s | FileCheck %s

Do not reindent string contents or change quoted Unicode attribute names.

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

```nix file:///flatten-attrs-preserve-values-2.nix
{
  a = {
    text = ''
      # literal text, not a comment
        indented
      ${"value"}
    '';
    "文" = "/* also literal */";
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
      "uri": "file:///flatten-attrs-preserve-values-2.nix"
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
CHECK: "newText": "  \n    a.text = ''\n      # literal text, not a comment\n        indented\n      ${\"value\"}\n    '';\n    a.\"文\" = \"/* also literal */\";\n  ",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 4,
CHECK-NEXT: "line": 8
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
