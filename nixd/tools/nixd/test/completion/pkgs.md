# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr='--nixpkgs-expr={ x = 1; y = 2; };' \
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


```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///completion.nix",
         "languageId":"nix",
         "version":1,
         "text":"pkgs"
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
            "character": 3
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

```
     CHECK:      "id": 1,
CHECK-NEXT:      "jsonrpc": "2.0",
CHECK-NEXT:      "result": {
CHECK-NEXT:        "isIncomplete": false,
CHECK-NEXT:        "items": []
CHECK-NEXT:      }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
