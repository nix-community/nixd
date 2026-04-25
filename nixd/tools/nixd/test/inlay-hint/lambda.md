# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ python3.version = \"3.13.6\"; ps.version = \"231\"; }" \
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
with pkgs; [
  (python3.withPackages (
    ps: [
      ps.numpy
      ps.pandas
    ]
  )) 
]
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
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "label": ": 3.13.6",
CHECK-NEXT:      "paddingLeft": false,
CHECK-NEXT:      "paddingRight": false,
CHECK-NEXT:      "position": {
CHECK-NEXT:        "character": 10,
CHECK-NEXT:        "line": 1
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
