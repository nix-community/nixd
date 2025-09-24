# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ hello.meta.description = \"Very Nice\";  }" \
# RUN: < %s | FileCheck %s


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
pkgs.hel
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
            "character": 7
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK:    "isIncomplete": false,
CHECK-NEXT:    "items": [
CHECK-NEXT:      {
CHECK-NEXT:        "data": "{\"MaxItems\":null,\"Prefix\":\"hel\",\"Scope\":[]}",
CHECK-NEXT:        "kind": 5,
CHECK-NEXT:        "label": "hello",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      }
CHECK-NEXT:    ]
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
