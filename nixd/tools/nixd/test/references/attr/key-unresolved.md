# RUN: nixd --lit-test < %s | FileCheck %s

The key `a.b` is in a set `a` whose value is not a set: unresolved, the error is unchanged.

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
let x = 1; in { a = x; a.b = 2; }
```

<-- textDocument/references(1) at 0:25, `let x = 1; in { a = x; a.b = 2; }`

```json
{
   "jsonrpc": "2.0",
   "id": 1,
   "method": "textDocument/references",
   "params": {
      "textDocument": {
         "uri": "file:///main.nix"
      },
      "position": {
         "line": 0,
         "character": 25
      },
      "context": {
         "includeDeclaration": false
      }
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT: "code": -32001,
CHECK-NEXT: "message": "references: cannot find variable on given node"
CHECK-NEXT: },
CHECK-NEXT: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
