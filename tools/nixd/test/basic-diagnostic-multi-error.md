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
         "text":"{ foo = 1; bar bar2 }"
      }
   }
}
```

```
     CHECK:         "message": "syntax error, unexpected ID, expecting end of file",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 9,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 9,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         },
CHECK-NEXT:         "severity": 1
CHECK-NEXT:       }
CHECK-NEXT:     ],
CHECK-NEXT:     "uri": "file:///basic.nix",
CHECK-NEXT:     "version": 1
CHECK-NEXT:   }
CHECK-NEXT: }
```



```json
{"jsonrpc":"2.0","method":"exit"}
```
