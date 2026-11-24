# RUN: nixd --lit-test < %s | FileCheck %s

With several names, a selection with `or` is not shared: its default can be a value of its own, which a copy would evaluate again: no action.

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

```nix file:///multi-select-or.nix
let e = { s = { a = 1; b = 2; }; }; in { inherit (e.s or { }) a b; }
```

<-- textDocument/codeAction(2) at 0:62

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///multi-select-or.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 62
         },
         "end": {
            "line": 0,
            "character": 62
         }
      },
      "context": {
         "diagnostics": [],
         "triggerKind": 2
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
 CHECK-NOT: "title": "Convert to explicit binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
