# RUN: nixd --lit-test < %s | FileCheck %s

`inherit x;` reads the scope outside the `rec` set; `x = y;` would read the set's own `y`, so the rename is refused.

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
let x = 1; in (rec { inherit x; y = 2; }).x
```

<-- textDocument/rename(1) at 0:4, `let x = 1; in (rec { inherit x; y = 2; }).x`

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
      "newName": "y"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot rename: `inherit` reads the variable from outside its `rec` or `let`, but the binding that replaces it would read the new name from that `rec` or `let`, which binds it"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
