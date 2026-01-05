# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action is NOT offered for single-segment paths.

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

```nix file:///pack-attrs-single.nix
{ foo = 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-single.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No Pack action should be offered for single-segment paths (nothing to pack).

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NOT:    "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
