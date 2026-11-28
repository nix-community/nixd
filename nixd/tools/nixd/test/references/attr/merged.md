# RUN: nixd --lit-test < %s | FileCheck %s

`a.b = 1; a.c = 2;` merge into one `a`: the second key finds `m.a`.

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

<-- textDocument/references(1) at 3:17, `m = { a.b = 1; a.c = 2; };`

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
         "line": 3,
         "character": 17
      },
      "context": {
         "includeDeclaration": false
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 9,
CHECK-NEXT: "line": 11
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 8,
CHECK-NEXT: "line": 11
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
CHECK-NEXT: ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
