# RUN: nixd --lit-test < %s | FileCheck %s

Test `remove extra with` action with multiline expression.

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

```nix file:///multiline-with.nix
with lib;
1 + 2
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///multiline-with.nix"
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

The action should remove the `with lib;` part.

```
     CHECK: "id": 2,
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "line": 0
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove `with` expression"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
