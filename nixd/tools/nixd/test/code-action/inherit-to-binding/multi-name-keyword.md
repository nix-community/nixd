# RUN: nixd --lit-test < %s | FileCheck %s

With several names and the cursor on `inherit`, no name is chosen: no action.

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

```nix file:///multi-keyword.nix
let e = { a = 1; b = 2; }; in { inherit (e) a b; }
```

<-- textDocument/codeAction(2) at 0:32

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///multi-keyword.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 32
         },
         "end": {
            "line": 0,
            "character": 32
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
