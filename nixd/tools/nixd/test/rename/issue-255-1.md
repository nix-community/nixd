# RUN: nixd --lit-test < %s | FileCheck %s

// https://github.com/nix-community/nixd/issues/255

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
         "text":"{a ? 1}: { x = a; }"
      }
   }
}
```

<-- textDocument/rename(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/rename",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
        "line": 0,
        "character":1
      },
      "newName": "b"
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "changes": {
CHECK-NEXT:     "file:///basic.nix": [
CHECK-NEXT:       {
CHECK-NEXT:         "newText": "b",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 16,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 15,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       {
CHECK-NEXT:         "newText": "b",
CHECK-NEXT:         "range": {
CHECK-NEXT:           "end": {
CHECK-NEXT:             "character": 2,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           },
CHECK-NEXT:           "start": {
CHECK-NEXT:             "character": 1,
CHECK-NEXT:             "line": 0
CHECK-NEXT:           }
CHECK-NEXT:         }
CHECK-NEXT:       }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
