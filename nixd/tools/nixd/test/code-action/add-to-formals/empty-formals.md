# RUN: nixd --lit-test < %s | FileCheck %s

Test `add to formals` action with empty formals `{ }`.

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

```nix file:///empty-formals.nix
{ }: y
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///empty-formals.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 5
         },
         "end":{
            "line":0,
            "character": 6
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should insert ` y ` after the opening `{` in empty formals.

```
     CHECK: "id": 2,
     CHECK: "newText": " y ",
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "add `y` to formals"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
