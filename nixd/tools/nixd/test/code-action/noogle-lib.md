
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

```nix file:///noogle-lib.nix
{ lib }: lib.mapAttrs
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///noogle-lib.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":9
         },
         "end":{
            "line":0,
            "character":17
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK:        "command": {
CHECK:          "arguments": [
CHECK:            "https://noogle.dev/f/lib/mapAttrs"
CHECK:          "command": "nixd.openNoogle",
CHECK:          "title": "Open lib.mapAttrs in noogle.dev"
CHECK:        "title": "Open lib.mapAttrs in noogle.dev"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
