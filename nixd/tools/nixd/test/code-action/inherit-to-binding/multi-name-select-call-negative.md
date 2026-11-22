# RUN: nixd --lit-test < %s | FileCheck %s

With several names, a selection from what is not a variable is not shared: a copy would evaluate `f e` again: no action.

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

```nix file:///multi-select-call.nix
let e = { s = { a = 1; b = 2; }; }; f = x: x; in { inherit ((f e).s) a b; }
```

<-- textDocument/codeAction(2) at 0:69

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///multi-select-call.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 69
         },
         "end": {
            "line": 0,
            "character": 69
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
