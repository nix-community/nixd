# RUN: nixd --lit-test < %s | FileCheck %s

<-- initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{
      "processId":123,
      "rootPath":"",
      "capabilities":{
      },
      "trace":"off"
   }
}
```

<-- textDocument/didOpen

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"let x = 1; in x"
      }
   }
}
```

Expected no diagnostics for this file:

```
     CHECK:   "method": "textDocument/publishDiagnostics",
CHECK-NEXT:   "params": {
CHECK-NEXT:     "diagnostics": [],
CHECK-NEXT:     "uri": "file:///basic.nix",
CHECK-NEXT:     "version": 1
CHECK-NEXT:   }
CHECK-NEXT: }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
