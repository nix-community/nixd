# RUN: nixd --lit-test < %s | FileCheck %s

Test that no action is offered when all formals are used.

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

```nix file:///all-used.nix
{x, y}: x + y
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///all-used.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 4
         },
         "end":{
            "line":0,
            "character": 5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No remove unused formal action should be offered since all formals are used.

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": []
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
