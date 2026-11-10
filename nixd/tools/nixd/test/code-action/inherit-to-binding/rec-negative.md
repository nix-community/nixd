# RUN: nixd --lit-test < %s | FileCheck %s

`inherit x;` in a `rec` set reads `x` from outside it; `x = x;` would read itself: no action.

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

```nix file:///rec.nix
let x = 1; in rec { inherit x; }
```

<-- textDocument/codeAction(2) at 0:28

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///rec.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 28
         },
         "end": {
            "line": 0,
            "character": 28
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
