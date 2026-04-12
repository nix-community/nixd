# RUN: nixd --lit-test \
# RUN: --nixos-options-expr='let roleType = { name = "enum"; description = "one of \"server\", \"agent\""; functor.payload.values = [ "server" "agent" ]; }; in { demo.role = { _type = "option"; type = roleType; }; }' \
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
{ demo.role = ""; }
```

<-- textDocument/completion(1)

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
            "character": 15
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK: "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "isIncomplete": false,
CHECK-NEXT:    "items": [
CHECK-NEXT:      {
CHECK-NEXT:        "data": "",
CHECK-NEXT:        "detail": "nixos | one of \"server\", \"agent\"",
CHECK-NEXT:        "kind": 20,
CHECK-NEXT:        "label": "server",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      },
CHECK-NEXT:      {
CHECK-NEXT:        "data": "",
CHECK-NEXT:        "detail": "nixos | one of \"server\", \"agent\"",
CHECK-NEXT:        "kind": 20,
CHECK-NEXT:        "label": "agent",
CHECK-NEXT:        "score": 0
CHECK-NEXT:      }
CHECK-NEXT:    ]
CHECK-NEXT:  }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
