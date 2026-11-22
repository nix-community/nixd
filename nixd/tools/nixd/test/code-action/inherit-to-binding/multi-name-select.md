# RUN: nixd --lit-test < %s | FileCheck %s

With several names, attributes selected from a variable are shared: the name under the cursor is converted.

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

```nix file:///multi-select.nix
let e = { s = { a = 1; b = 2; }; }; in { inherit (e.s) a b; }
```

<-- textDocument/codeAction(2) at 0:55

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///multi-select.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 55
         },
         "end": {
            "line": 0,
            "character": 55
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
     CHECK: "newText": "",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 56,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 54,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": " a = e.s.a;",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 59,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 59,
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
