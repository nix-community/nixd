# RUN: nixd --lit-test < %s | FileCheck %s

The variable is inherited again inside `E` of the `inherit (E) f;` that defines it, so the binding that replaces that inherit would have to split the inner one too: refused.

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
let inherit ({ x = { inherit f; }; f = 1; }) f; in f
```

<-- textDocument/rename(1) at 0:51, `let inherit ({ x = { inherit f; }; f = 1; }) f; in f`

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
         "character": 51
      },
      "newName": "g"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot rename: the variable is inherited inside the expression of an `inherit (...)` that has to be rewritten"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
