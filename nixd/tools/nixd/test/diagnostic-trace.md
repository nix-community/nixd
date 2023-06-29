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
         "text":"rec { a = x; x = a; }.a"
      }
   }
}
```


```
     CHECK:        "message": "while evaluating the attribute 'a'",
CHECK-NEXT:        "range": {
CHECK-NEXT:          "end": {
CHECK-NEXT:            "character": 6,
CHECK-NEXT:            "line": 0
CHECK-NEXT:          },
CHECK-NEXT:          "start": {
CHECK-NEXT:            "character": 6,
CHECK-NEXT:            "line": 0
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        "severity": 3
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
