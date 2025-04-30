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
let x = 1; y = 2; in x + y
```

<-- textDocument/documentHighlight(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/documentHighlight",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
        "line": 0,
        "character":4
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK-NEXT:   {
CHECK-NEXT:     "kind": 2,
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 22,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 21,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     }
CHECK-NEXT:   },
CHECK-NEXT:   {
CHECK-NEXT:     "kind": 3,
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 5,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 4,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     }
CHECK-NEXT:   }
CHECK-NEXT: ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
