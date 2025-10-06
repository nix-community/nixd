# RUN: nixd --nixpkgs-expr='{ ax = 1; ay = 2; }' --lit-test < %s | FileCheck %s

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


```nix file:///completion.nix
with pkgs; [ a ]
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file:///completion.nix"
        },
        "position": {
            "line": 0,
            "character": 13
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK:  "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "isIncomplete": false,
CHECK-NEXT:    "items": [
CHECK-NEXT:      {
CHECK-NEXT:        "data": "",
CHECK-NEXT:        "kind": 14,
CHECK-NEXT:        "label": "abort",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "data": "{\"MaxItems\":null,\"Prefix\":\"a\",\"Scope\":[]}",
CHECK-NEXT:        "kind": 5,
CHECK-NEXT:        "label": "ax",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "data": "{\"MaxItems\":null,\"Prefix\":\"a\",\"Scope\":[]}",
CHECK-NEXT:        "kind": 5,
CHECK-NEXT:        "label": "ay",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      }
CHECK-NEXT:    ]
CHECK-NEXT:  }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
