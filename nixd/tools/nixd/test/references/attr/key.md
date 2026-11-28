# RUN: nixd --lit-test < %s | FileCheck %s

References asked at the key: selections, `?`, `or` and `inherit (s)`.

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

<-- textDocument/references(1) at 1:8, `s = { level = 2; nested.deep = 1; };`

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
         "line": 1,
         "character": 8
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
CHECK-NEXT: "character": 13,
CHECK-NEXT: "line": 6
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 8,
CHECK-NEXT: "line": 6
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 15,
CHECK-NEXT: "line": 8
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 10,
CHECK-NEXT: "line": 8
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 13,
CHECK-NEXT: "line": 9
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 8,
CHECK-NEXT: "line": 9
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: },
CHECK-NEXT: {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 19,
CHECK-NEXT: "line": 12
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 14,
CHECK-NEXT: "line": 12
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
CHECK-NEXT: ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
