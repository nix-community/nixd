# RUN: nixd --nixpkgs-expr="{ x.meta.position = \"/foo:33\"; }" --lit-test < %s | FileCheck %s

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
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"with pkgs; x"
      }
   }
}
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
        "character":11
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": [
CHECK-NEXT:   {
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
CHECK-NEXT:   },
CHECK-NEXT:   {
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 4,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "uri": "file:///basic.nix"
CHECK-NEXT:   }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
