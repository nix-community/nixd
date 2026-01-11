# RUN: nixd --lit-test < %s | FileCheck %s

Test `with` to `let/inherit` with attribute set body.

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

```nix file:///with-to-let-attrset.nix
with lib; { foo = optionalString true "x"; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-attrset.nix"
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

Body is an attribute set.

```
     CHECK: "id": 2,
     CHECK: "newText": "let inherit (lib) optionalString; in { foo = optionalString true \"x\"; }",
     CHECK: "kind": "refactor.rewrite",
     CHECK: "title": "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
