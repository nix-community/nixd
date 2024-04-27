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
         "text":"{x, y}: x + 1"
      }
   }
}
```

```
     CHECK: "diagnostics": [
CHECK-NEXT:   {
CHECK-NEXT:     "code": "sema-def-not-used",
CHECK-NEXT:     "message": "definition `y` is not used",
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 5,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 4,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "relatedInformation": [],
CHECK-NEXT:     "severity": 2,
CHECK-NEXT:     "source": "nixf",
CHECK-NEXT:     "tags": [
CHECK-NEXT:       1
CHECK-NEXT:     ]
CHECK-NEXT:   }
CHECK-NEXT: ],
CHECK-NEXT: "uri": "file:///basic.nix",
```
