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
         "text":"with builtins; foo + bar"
      }
   }
}
```

<-- textDocument/references(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/references",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
        "line": 0,
        "character":3
      }
   }
}
```

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 18,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 15,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "uri": "file:///basic.nix"
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 24,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 21,
CHECK-NEXT:          "line": 0
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "uri": "file:///basic.nix"
CHECK-NEXT:    }
CHECK-NEXT:  ]
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
