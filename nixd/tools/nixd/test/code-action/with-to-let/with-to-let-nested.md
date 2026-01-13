# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered for outer `with` in nested `with` expressions.

Converting outer `with` to `let/inherit` can change variable resolution semantics
because `let` bindings shadow inner `with` scopes. To avoid this semantic issue,
the code action is only offered for the innermost `with` in a nested chain.

See: https://github.com/nix-community/nixd/pull/768#discussion_r2679465713

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

```nix file:///with-to-let-nested.nix
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
         "uri":"file:///with-to-let-nested.nix"
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

No "Convert with to let/inherit" action should be offered when cursor is
on the outer `with` of a nested chain.

```
     CHECK:   "id": 2,
CHECK-NOT:   "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
