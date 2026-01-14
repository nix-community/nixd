# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered for middle `with` in a triple-nested chain.

In `with a; with b; with c; x`, only the innermost `with c` should get the action.
Both `with a` and `with b` have directly nested `with` expressions in their bodies.

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

```nix file:///with-to-let-nested-triple.nix
with a; with b; with c; foo
```

<-- textDocument/codeAction(2)

Request code action on middle `with b` (character 8-12).

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-nested-triple.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":8
         },
         "end":{
            "line":0,
            "character":12
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No "Convert with to let/inherit" action should be offered for middle `with b`
because its body contains another `with c`.

```
     CHECK:   "id": 2,
CHECK-NOT:   "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
