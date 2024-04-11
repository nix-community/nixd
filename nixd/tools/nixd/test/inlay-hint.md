# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ hello.version = \"0.3.12\";  }" \
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

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"with pkgs; [ hello  ]"
      }
   }
}
```



```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/inlayHint",
    "params": {
        "textDocument":{
            "uri":"file:///basic.nix"
        },
        "range": {
          "start":{
            "line": 0,
            "character":0
          },
          "end":{
            "line":0,
            "character":20
          }
        }
    }
}
```

```
     CHECK:  "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "label": ": 0.3.12",
CHECK-NEXT:      "paddingLeft": false,
CHECK-NEXT:      "paddingRight": false,
CHECK-NEXT:      "position": {
CHECK-NEXT:        "character": 18,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      }
CHECK-NEXT:    }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
