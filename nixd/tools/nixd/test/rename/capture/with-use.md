# RUN: nixd --lit-test < %s | FileCheck %s

(b) A use of `y` read from `with` would read the renamed lexical binding instead (lexical wins): refused.

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
let x = 1; in with { y = 2; }; y + x
```

<-- textDocument/rename(1) at 0:4, `let x = 1; in with { y = 2; }; y + x`

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
CHECK-NEXT: "message": "cannot rename: the use of `y` at line 1, column 32 reads `y` from the `with` at line 1, column 15, but would read the renamed `x`"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
