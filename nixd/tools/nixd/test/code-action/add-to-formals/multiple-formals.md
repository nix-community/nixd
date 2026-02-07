# RUN: nixd --lit-test < %s | FileCheck %s

Test `add to formals` action with multiple existing formals.

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

```nix file:///multiple-formals.nix
{a, b, c}: d
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///multiple-formals.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 11
         },
         "end":{
            "line":0,
            "character": 12
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should insert `, d` after the last formal `c`.

```
     CHECK: "id": 2,
     CHECK: "newText": ", d",
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "add `d` to formals"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
