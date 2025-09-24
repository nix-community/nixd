# RUN: nixd --nixpkgs-expr='{ ax.bx = 1; ax.by = 2; }' --lit-test < %s | FileCheck %s

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
with pkgs; [ ax.b ]
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
            "character": 16
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "isIncomplete": false,
CHECK-NEXT:   "items": [
CHECK-NEXT:     {
CHECK-NEXT:       "data": "{\"MaxItems\":null,\"Prefix\":\"b\",\"Scope\":[\"ax\"]}",
CHECK-NEXT:       "kind": 5,
CHECK-NEXT:       "label": "bx",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK-NEXT:       "data": "{\"MaxItems\":null,\"Prefix\":\"b\",\"Scope\":[\"ax\"]}",
CHECK-NEXT:       "kind": 5,
CHECK-NEXT:       "label": "by",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     }
CHECK-NEXT:   ]
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
