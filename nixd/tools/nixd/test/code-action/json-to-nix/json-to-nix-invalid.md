# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action is NOT offered for invalid JSON text.

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

```nix file:///json-to-nix-invalid.nix
{not valid json}
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-invalid.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":16
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Invalid JSON should NOT offer the "Convert JSON to Nix" action.

```
     CHECK:   "id": 1,
     CHECK:   "result": [
 CHECK-NOT:   "title": "Convert JSON to Nix"
     CHECK:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
