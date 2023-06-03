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


Check that we do eval the file. Nix files are legal expressions, and should be evaluable.


```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///test-eval.nix",
         "languageId":"nix",
         "version":1,
         "text":"let x = 1; in y"
      }
   }
}
```

```
     CHECK:         "message": "undefined variable 'y'",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 14,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 14,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         },
CHECK-NEXT:         "severity": 1
CHECK-NEXT:       }
CHECK-NEXT:     ],
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
