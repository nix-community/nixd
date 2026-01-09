# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action is NOT offered for empty JSON arrays
(since [] is already valid Nix and conversion would be trivial).

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

```nix file:///json-to-nix-empty-array.nix
[]
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-empty-array.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":2
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Empty JSON array [] is already valid Nix, so no conversion action is offered.

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": []
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
