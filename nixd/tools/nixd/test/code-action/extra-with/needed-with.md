# RUN: nixd --lit-test < %s | FileCheck %s

Test that `remove extra with` action is NOT offered when with is actually used.

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

```nix file:///needed-with.nix
with lib; foo
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///needed-with.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":4
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No "remove `with` expression" action should be offered because `foo` is
resolved through the `with lib` scope.

```
         CHECK: "id": 2,
     CHECK-NOT: "remove `with` expression"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
