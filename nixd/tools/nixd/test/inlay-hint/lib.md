# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ lib.version = \"25.11pre851662.84c256e42600\"; }" \
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

```nix file:///basic.nix
with pkgs; [ lib ]
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
            "line":7,
            "character":1
          }
        }
    }
}
```

```
     CHECK:  "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": []
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
