
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

```nix file:///unused-formal.nix
{x, y}: x
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///unused-formal.nix"
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
CHECK-NEXT:           "file:///unused-formal.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": "",
CHECK-NEXT:               "range": {
CHECK:                      "line": 0
CHECK:                    },
CHECK:                    "start": {
CHECK:                      "line": 0
CHECK:                    }
CHECK:                  }
CHECK:                }
CHECK:              ]
CHECK:            }
CHECK:          },
CHECK:          "kind": "quickfix",
CHECK:          "title": "remove unused formal"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
