# RUN: nixd --lit-test < %s | FileCheck %s

`r.b` goes to the key `b` of the `rec` set.

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
  t = s;
in
{
  x = s.level;
  y = (s).nested.deep;
  z = t.level;
  w = s ? level;
  v = s.missing or 3;
  u = r.b;
  inherit (s) level;
  f = arg: arg.level;
}
```

<-- textDocument/definition(1) at 11:8, `u = r.b;`

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
         "line": 11,
         "character": 8
      }
   }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT: "range": {
CHECK-NEXT: "end": {
CHECK-NEXT: "character": 20,
CHECK-NEXT: "line": 2
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 19,
CHECK-NEXT: "line": 2
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
