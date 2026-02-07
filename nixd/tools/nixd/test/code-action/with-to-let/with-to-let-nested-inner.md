# RUN: nixd --lit-test < %s | FileCheck %s

Test that the code action IS offered for the innermost `with` in nested chains.

The innermost `with` is safe to convert because its `let` bindings would shadow
outer `with` scopes anyway, preserving the original variable resolution semantics.

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

```nix file:///with-to-let-nested-inner.nix
with outer; with inner; foo bar
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-nested-inner.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":12
         },
         "end":{
            "line":0,
            "character":16
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should convert the inner `with` to `let/inherit`, preserving the
outer `with` in place.

```
     CHECK: "id": 2,
     CHECK: "newText": "let inherit (inner) bar foo; in foo bar",
     CHECK: "kind": "refactor.rewrite",
     CHECK: "title": "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
