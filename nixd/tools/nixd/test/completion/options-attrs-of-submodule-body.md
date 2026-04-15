# RUN: nixd --lit-test \
# RUN: --nixos-options-expr='let subOptions = { baz = { _type = "option"; description = "baz option"; type = { name = "baz-type"; description = "baz type"; }; }; qux = { _type = "option"; description = "qux option"; type = { name = "qux-type"; description = "qux type"; }; }; }; in { foo.bar = { _type = "option"; type = { name = "attrsOf"; nestedTypes.elemType = { name = "submodule"; getSubOptions = _: subOptions; }; }; }; }' \
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
{
  foo.bar.x = {
    
  };
}
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
            "line": 2,
            "character": 4
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
CHECK-NEXT:       "detail": "nixos | baz-type (baz type)",
CHECK-NEXT:       "documentation": {
CHECK-NEXT:         "kind": "markdown",
CHECK-NEXT:         "value": "baz option"
CHECK-NEXT:       },
CHECK-NEXT:       "insertText": "baz = ;",
CHECK-NEXT:       "insertTextFormat": 1,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "baz",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK-NEXT:       "data": "",
CHECK-NEXT:       "detail": "nixos | qux-type (qux type)",
CHECK-NEXT:       "documentation": {
CHECK-NEXT:         "kind": "markdown",
CHECK-NEXT:         "value": "qux option"
CHECK-NEXT:       },
CHECK-NEXT:       "insertText": "qux = ;",
CHECK-NEXT:       "insertTextFormat": 1,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "qux",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     }
CHECK-NEXT:   ]
CHECK-NEXT: }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
