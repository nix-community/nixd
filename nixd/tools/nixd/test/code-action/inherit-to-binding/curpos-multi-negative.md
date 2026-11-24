# RUN: nixd --lit-test < %s | FileCheck %s

The same with several names.

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

```nix file:///curpos-multi.nix
let __curPos = 1; x = 2; in { inherit __curPos x; }
```

<-- textDocument/codeAction(2) at 0:38

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/codeAction",
   "params": {
      "textDocument": {
         "uri": "file:///curpos-multi.nix"
      },
      "range": {
         "start": {
            "line": 0,
            "character": 38
         },
         "end": {
            "line": 0,
            "character": 38
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
