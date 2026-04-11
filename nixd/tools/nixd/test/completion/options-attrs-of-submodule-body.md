# RUN: nixd --lit-test \
# RUN: --nixos-options-expr='let subOptions = { home = { _type = "option"; description = "Home directory."; type = { name = "path"; description = "absolute path"; }; }; isNormalUser = { _type = "option"; description = "Normal user."; type = { name = "bool"; description = "boolean"; }; }; }; in { users.users = { _type = "option"; type = { name = "attrsOf"; nestedTypes.elemType = { name = "submodule"; getSubOptions = _: subOptions; }; }; }; }' \
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
  users.users.alice = {
    
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
CHECK-NEXT:       "detail": "nixos | path (absolute path)",
CHECK-NEXT:       "documentation": {
CHECK-NEXT:         "kind": "markdown",
CHECK-NEXT:         "value": "Home directory."
CHECK-NEXT:       },
CHECK-NEXT:       "insertText": "home = ;",
CHECK-NEXT:       "insertTextFormat": 1,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "home",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK-NEXT:       "data": "",
CHECK-NEXT:       "detail": "nixos | bool (boolean)",
CHECK-NEXT:       "documentation": {
CHECK-NEXT:         "kind": "markdown",
CHECK-NEXT:         "value": "Normal user."
CHECK-NEXT:       },
CHECK-NEXT:       "insertText": "isNormalUser = ;",
CHECK-NEXT:       "insertTextFormat": 1,
CHECK-NEXT:       "kind": 4,
CHECK-NEXT:       "label": "isNormalUser",
CHECK-NEXT:       "score": 0
CHECK-NEXT:     }
CHECK-NEXT:   ]
CHECK-NEXT: }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
