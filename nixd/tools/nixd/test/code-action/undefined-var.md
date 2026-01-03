
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

```nix file:///undefined-var.nix
{x}: y
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///undefined-var.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":4
         },
         "end":{
            "line":0,
            "character":5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
CHECK-NEXT:       "edit": {
CHECK-NEXT:         "changes": {
CHECK-NEXT:           "file:///undefined-var.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": ", y",
CHECK-NEXT:               "range": {
CHECK-NEXT:                 "end": {
CHECK-NEXT:                   "character": 2,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 },
CHECK-NEXT:                 "start": {
CHECK-NEXT:                   "character": 2,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 }
CHECK-NEXT:               }
CHECK-NEXT:             }
CHECK-NEXT:           ]
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       "isPreferred": true,
CHECK-NEXT:       "kind": "quickfix",
CHECK-NEXT:       "title": "add `y` to formals"
CHECK-NEXT:     },
CHECK:        "title": "Extract expression to file"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
