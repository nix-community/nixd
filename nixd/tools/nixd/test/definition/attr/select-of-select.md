# RUN: nixd --lit-test < %s | FileCheck %s

`t = s.nested;` is itself a selection: `t.deep` goes through it to `deep`.

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
  s = { nested = { deep = 1; }; };
  t = s.nested;
in t.deep
```

<-- textDocument/definition(1) at 3:5, `in t.deep`

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
         "line": 3,
         "character": 5
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
CHECK-NEXT: "character": 23,
CHECK-NEXT: "line": 1
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 19,
CHECK-NEXT: "line": 1
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
