# RUN: nixd --lit-test < %s | FileCheck %s

`s.value = ...` is merged into the `rec` set, so `value` inside it is `s.value`, not the outer `value`.

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
  s = rec { selected = value; };
  s.value = { leaf = 1; };
  value = { leaf = 2; };
in [ s.selected.leaf value.leaf ]
```

<-- textDocument/definition(1) at 4:16, `in [ s.selected.leaf value.leaf ]`

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
         "line": 4,
         "character": 16
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
CHECK-NEXT: "character": 18,
CHECK-NEXT: "line": 2
CHECK-NEXT: },
CHECK-NEXT: "start": {
CHECK-NEXT: "character": 14,
CHECK-NEXT: "line": 2
CHECK-NEXT: }
CHECK-NEXT: },
CHECK-NEXT: "uri": "file:///main.nix"
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
