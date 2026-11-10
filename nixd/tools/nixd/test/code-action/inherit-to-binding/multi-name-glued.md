# RUN: nixd --lit-test < %s | FileCheck %s

Removing a name written without spaces around it (`a"b"c`) keeps its neighbours apart.

<-- initialize(0)

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


<-- textDocument/didOpen

```nix file:///glued.nix
let e = { a = 1; b = 2; c = 3; }; in { inherit (e) a"b"c; }
```

<-- textDocument/codeAction(2) at 0:53

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///glued.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 53
         },
         "end": {
            "line": 0,
            "character": 53
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
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
     CHECK: "newText": " ",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 55,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 52,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": " b = e.b;",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 57,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 57,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: ]
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "kind": "refactor.rewrite",
CHECK-NEXT: "title": "Convert to explicit binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
