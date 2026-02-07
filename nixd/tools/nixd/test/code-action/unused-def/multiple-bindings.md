# RUN: nixd --lit-test < %s | FileCheck %s

Test `remove unused binding` action with multiple bindings where only one is unused.

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

```nix file:///multiple-bindings.nix
let x = 1; y = 2; in y
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///multiple-bindings.nix"
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

The action should remove only `x = 1;` leaving `let y = 2; in y`.

```
     CHECK: "id": 2,
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "character": 10,
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "character": 4,
     CHECK:     "line": 0
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove unused binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
