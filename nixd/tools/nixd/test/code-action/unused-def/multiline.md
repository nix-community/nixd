# RUN: nixd --lit-test < %s | FileCheck %s

Test `remove unused binding` action with multiline let expression.

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

```nix file:///multiline.nix
let
  unused = 1;
in
  2
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///multiline.nix"
      },
      "range":{
         "start":{
            "line": 1,
            "character": 2
         },
         "end":{
            "line":1,
            "character": 8
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should remove the entire binding line.

```
     CHECK: "id": 2,
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "line": 1
     CHECK:   "start":
     CHECK:     "line": 1
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove unused binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
