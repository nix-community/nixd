
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

```nix file:///with-to-let.nix
{ lib }: with lib; { x = mapAttrs; }
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":9
         },
         "end":{
            "line":0,
            "character":13
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
CHECK:        "newText": "let inherit (lib) mapAttrs; in { x = mapAttrs; }",
CHECK:        "title": "Convert with to let/inherit"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
