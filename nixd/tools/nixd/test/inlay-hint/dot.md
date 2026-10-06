# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ nerd-fonts.fira-code.version = \"3.4.0\"; }" \
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
with pkgs; [ nerd-fonts.fira-code ]
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
            "character":35
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
CHECK-NEXT:      "label": ": 3.4.0",
CHECK-NEXT:      "paddingLeft": false,
CHECK-NEXT:      "paddingRight": false,
CHECK-NEXT:      "position": {
CHECK-NEXT:        "character": 33,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
