# RUN: nixd --lit-test < %s | FileCheck %s

With another name kept in the `inherit`, the split would copy `E`, an attribute set the other name keeps reading, so it would be evaluated twice: refused.

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
