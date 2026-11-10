# RUN: nixd --lit-test < %s | FileCheck %s

A `let` variable from `inherit __curPos;` cannot be split out: `y = __curPos;` would be the position it is written at, not the variable: refused.

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
let __curPos = 1; in let inherit __curPos; in { inherit __curPos; }
```

<-- textDocument/rename(1) at 0:56, `let __curPos = 1; in let inherit __curPos; in { inherit __curPos; }`

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
         "character": 56
      },
      "newName": "y"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot rename: the binding that replaces `inherit __curPos;` would read the current position, not the variable `__curPos`"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
