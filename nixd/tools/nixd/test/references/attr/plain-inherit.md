# RUN: nixd --lit-test < %s | FileCheck %s

`inherit x;` has no set to select from: it is skipped, and the selection is found.

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

```nix file:///main.nix
let
  x = 1;
  s = { level = 2; };
in { inherit x; v = s.level; }
```

<-- textDocument/references(1) at 3:22, `in { inherit x; v = s.level; }`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/references",
   "params": {
      "textDocument": {
         "uri": "file:///main.nix"
      },
      "position": {
         "line": 3,
         "character": 22
      },
      "context": {
         "includeDeclaration": false
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 27,
CHECK-NEXT: "line": 3
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 22,
CHECK-NEXT: "line": 3
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
CHECK-NEXT: ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
