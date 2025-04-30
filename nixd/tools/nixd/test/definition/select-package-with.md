# RUN: nixd --nixpkgs-expr="{ x.y.meta.position = \"/foo:33\"; }" --lit-test < %s | FileCheck %s

Similar to [package.md](./package.md), but testing that we can do "selection" + "with".
i.e. testing if `with pkgs; [x.y]` works.

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

```nix file:///basic.nix
with pkgs; [x.y]
```

<-- textDocument/definition(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
        "line": 0,
        "character":15
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 33
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 33
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "uri": "file:///foo"
CHECK-NEXT:   }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
