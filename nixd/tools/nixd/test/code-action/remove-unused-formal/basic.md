# RUN: nixd --lit-test < %s | FileCheck %s

Test basic `remove unused formal` action.

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

```nix file:///basic.nix
{x, y}: x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
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

The action should remove `, y` from the formals since `y` is unused.

```
     CHECK: "id": 2,
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove unused formal `y`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
