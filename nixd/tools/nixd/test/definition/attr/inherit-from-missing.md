# RUN: nixd --lit-test < %s | FileCheck %s

`inherit (s) zzz;` names an attribute `s` does not have: no definition.

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
let s = { a = 1; }; in { inherit (s) zzz; }
```

<-- textDocument/definition(1) at 0:37, `let s = { a = 1; }; in { inherit (s) zzz; }`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/definition",
   "params": {
      "textDocument": {
         "uri": "file:///main.nix"
      },
      "position": {
         "line": 0,
         "character": 37
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": null
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
