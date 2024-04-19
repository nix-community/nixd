# RUN: nixd --nixpkgs-expr='{ a = 1; b = 2; }' --lit-test < %s | FileCheck %s

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
         "uri":"file:///completion.nix",
         "languageId":"nix",
         "version":1,
         "text":"with pkgs; [  ]"
      }
   }
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
            "line": 0,
            "character": 14
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK:    "kind": 5,
CHECK-NEXT:    "label": "a",
CHECK-NEXT:    "score": 0
CHECK-NEXT:  },
CHECK-NEXT:  {
CHECK-NEXT:    "data": "{\"Prefix\":\"\",\"Scope\":[]}",
CHECK-NEXT:    "kind": 5,
CHECK-NEXT:    "label": "b",
CHECK-NEXT:    "score": 0
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
