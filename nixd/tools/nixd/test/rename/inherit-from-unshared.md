# RUN: nixd --lit-test < %s | FileCheck %s

Splitting `a` out of `inherit (E) a b;` would copy `E`, evaluated again as a value of its own (`a == b` turns false): refused.

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
let inherit (let v = { f = x: x; }; in { a = v; b = v; }) a b; in a == b
```

<-- textDocument/rename(1) at 0:66, `let inherit (let v = { f = x: x; }; in { a = v; b = v; }) a b; in a == b`

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
         "character": 66
      },
      "newName": "c"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot rename: the binding that replaces the name would copy the expression of `inherit (...)`, which the other names keep reading; the copy would be evaluated again, as a value of its own"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
