# RUN: nixd --lit-test < %s | FileCheck %s

Test removing the first formal (special case for comma handling).

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

```nix file:///first-formal.nix
{x, y}: y
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///first-formal.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 1
         },
         "end":{
            "line":0,
            "character": 2
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should remove `x, ` from the formals since `x` is unused.

```
     CHECK: "id": 2,
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove unused formal `x`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
