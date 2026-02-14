# RUN: nixd --lit-test < %s | FileCheck %s

Test basic `remove extra with` action for unused with expression.

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

```nix file:///extra-with.nix
with {}; 1
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///extra-with.nix"
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

The action should remove `with {};` leaving just `1`.

```
     CHECK: "id": 2,
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "character": 4,
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "character": 0,
     CHECK:     "line": 0
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove `with` expression"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
