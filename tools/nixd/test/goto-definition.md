# RUN: nixd --lit-test < %s | FileCheck %s

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

Testing this nix file:

```nix
let
  pkgs = {
    a = 1;
  };
in
with pkgs;

a
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///with.nix",
         "languageId":"nix",
         "version":1,
         "text":"let\n  pkgs = {\n    a = 1;\n  };\nin\nwith pkgs;\n\na\n\n"
      }
   }
}
```

<-- textDocument/definition

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///with.nix"
      },
      "position":{
         "line":6,
         "character":5
      }
   }
}
```


```
     CHECK: "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 2,
CHECK-NEXT:         "line": 1
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 2,
CHECK-NEXT:         "line": 1
CHECK-NEXT:       }
CHECK-NEXT:     },
CHECK-NEXT:     "uri": "file:///with.nix"
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
