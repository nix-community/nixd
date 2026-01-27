# RUN: nixd --lit-test < %s | FileCheck %s

Test `add to formals` action with nested lambdas - should add to innermost enclosing lambda.

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

```nix file:///nested-lambda.nix
{x}: {y}: z
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///nested-lambda.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 10
         },
         "end":{
            "line":0,
            "character": 11
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should add `, z` to the inner lambda's formals (after `y`).

```
     CHECK: "id": 2,
     CHECK: "newText": ", z",
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "add `z` to formals"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
