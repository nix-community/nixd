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

```nix file:///basic.nix
{x, y}: x + 1
```

```
     CHECK: "diagnostics": [
CHECK-NEXT:   {
CHECK-NEXT:     "code": "sema-unused-def-lambda-noarg-formal",
CHECK-NEXT:     "message": "attribute `y` of argument is not used",
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
