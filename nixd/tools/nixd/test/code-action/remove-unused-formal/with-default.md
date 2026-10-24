# RUN: nixd --lit-test < %s | FileCheck %s

Test removing unused formal with default value.

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

```nix file:///with-default.nix
{x, y ? 1}: x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-default.nix"
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

The action should remove `, y ? 1` from the formals since `y` is unused.

```
     CHECK: "id": 2,
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove unused formal `y`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
