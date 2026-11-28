# RUN: nixd --lit-test < %s | FileCheck %s

(a) A `rec` set between the use and the definition binds the new name: refused.

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
let x = 1; in (rec { y = 2; z = x; }).z
```

<-- textDocument/rename(1) at 0:4, `let x = 1; in (rec { y = 2; z = x; }).z`

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
CHECK-NEXT: "message": "cannot rename: the use of `x` at line 1, column 33 would read the `y` bound at line 1, column 22 instead"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
