# RUN: nixd --lit-test < %s | FileCheck %s

A variable bound by a `rec` set with `__overrides` is not an attribute to resolve: its uses are found as for any variable.

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
rec { __overrides = { }; a = 1; b = a; }
```

<-- textDocument/references(1) at 0:36, `rec { __overrides = { }; a = 1; b = a; }`

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
         "line": 0,
         "character": 36
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
CHECK-NEXT: "character": 37,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 36,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
CHECK-NEXT: ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
