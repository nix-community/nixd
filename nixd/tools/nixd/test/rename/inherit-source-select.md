# RUN: nixd --lit-test < %s | FileCheck %s

`E` as the base of a selection: `inherit (libs.helpers) triple;`.

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
let libs = { helpers = { triple = 3; }; }; inherit (libs.helpers) triple; in triple
```

<-- textDocument/rename(1) at 0:4, `let libs = { helpers = { triple = 3; }; }; inherit (libs.helpers) triple; in triple`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/rename",
   "params": {
      "textDocument": {
         "uri": "file:///main.nix"
      },
      "position": {
         "line": 0,
         "character": 4
      },
      "newName": "library"
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT: "changes": {
CHECK-NEXT: "file:///main.nix": [
CHECK-NEXT: {
CHECK-NEXT: "newText": "library",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 56,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 52,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": "library",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 8,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 4,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: ]
CHECK-NEXT: }
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
