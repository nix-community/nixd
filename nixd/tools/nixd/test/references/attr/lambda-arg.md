# RUN: nixd --lit-test < %s | FileCheck %s

A lambda argument is unresolved: the error is unchanged.

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
  s = { level = 2; nested.deep = 1; };
  r = rec { a = 1; b = a; };
  m = { a.b = 1; a.c = 2; };
in
{
  x = s.level;
  y = (s).nested.deep;
  w = s ? level;
  v = s.level or 3;
  u = r.a;
  k = m.a.c;
  inherit (s) level;
  settings = { level = 2; };
  f = arg: arg.level;
}
```

<-- textDocument/references(1) at 14:15, `f = arg: arg.level;`

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
         "line": 14,
         "character": 15
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
