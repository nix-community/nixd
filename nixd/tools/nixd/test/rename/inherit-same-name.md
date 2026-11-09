# RUN: nixd --lit-test < %s | FileCheck %s

Renaming to the same name changes nothing; splitting `inherit x;` into `x = x;` would recurse.

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
let x = 1; in let inherit x; in x
```

<-- textDocument/rename(1) at 0:32, `let x = 1; in let inherit x; in x`

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
         "character": 32
      },
      "newName": "x"
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT: "changes": {
CHECK-NEXT: "file:///main.nix": []
CHECK-NEXT: }
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
