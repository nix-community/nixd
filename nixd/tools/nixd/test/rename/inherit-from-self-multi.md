# RUN: nixd --lit-test < %s | FileCheck %s

The same with another name kept in the `inherit`.

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
let inherit ({ f = n: if n == 0 then h else f (n - 1); h = 0; }) f h; in f 2
```

<-- textDocument/rename(1) at 0:73, `let inherit ({ f = n: if n == 0 then h else f (n - 1); h = 0; }) f h; in f 2`

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
         "character": 73
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
CHECK-NEXT: "character": 45,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 44,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": "g",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 74,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 73,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": "",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 67,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 65,
CHECK-NEXT: "line": 0
CHECK-NEXT: }
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "newText": " g = ({ f = n: if n == 0 then h else g (n - 1); h = 0; }).f;",
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 69,
CHECK-NEXT: "line": 0
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 69,
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
