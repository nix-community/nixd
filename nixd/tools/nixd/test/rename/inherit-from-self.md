# RUN: nixd --lit-test < %s | FileCheck %s

Uses of the variable inside `inherit (e)`'s own expression are renamed inside the binding that replaces it, without overlapping edits.

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
let inherit ({ f = n: if n == 0 then 0 else f (n - 1); }) f; in f 2
```

<-- textDocument/rename(1) at 0:64, `let inherit ({ f = n: if n == 0 then 0 else f (n - 1); }) f; in f 2`

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
         "character": 64
      },
      "newName": "g"
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
CHECK-NEXT: "newText": "g",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 65,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 64,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": "g = ({ f = n: if n == 0 then 0 else g (n - 1); }).f;",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 60,
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
