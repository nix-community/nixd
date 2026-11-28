# RUN: nixd --lit-test < %s | FileCheck %s

A key of the set the file returns cannot be renamed.

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
let
  s = { level = 2; };
in
{
  x = s.level;
  settings = { level = 2; };
}
```

<-- textDocument/rename(1) at 5:15, `settings = { level = 2; };`

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
         "line": 5,
         "character": 15
      },
      "newName": "renamed"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "cannot find variable on given node"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
