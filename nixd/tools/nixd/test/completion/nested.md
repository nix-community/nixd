# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ foo.bar = { _type = \"option\"; }; }" \
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
      "capabilities": {
      },
      "trace":"off"
   }
}
```


<-- textDocument/didOpen


```nix file:///completion.nix
{ bar = 1; foo = { ba = }; }
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
            "character": 20
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
CHECK-NEXT:       "data": "",
CHECK-NEXT:       "detail": "nixos | ? (missing type)",
CHECK-NEXT:       "documentation": null,
CHECK-NEXT:       "insertText": "bar = ;",
CHECK-NEXT:       "insertTextFormat": 1,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "bar",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     }
CHECK-NEXT:   ]
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
