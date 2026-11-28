# RUN: nixd --lit-test < %s | FileCheck %s

A `rec` set that is the value of a key resolves its own variables from the outermost set: `alias = value` goes to `{ deep = 1; }`.

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
  outer = { s = rec { value = { deep = 1; }; alias = value; }; };
in outer.s.alias.deep
```

<-- textDocument/definition(1) at 2:17, `in outer.s.alias.deep`

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
         "line": 2,
         "character": 17
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
CHECK-NEXT: "character": 36,
CHECK-NEXT: "line": 1
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 32,
CHECK-NEXT: "line": 1
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
