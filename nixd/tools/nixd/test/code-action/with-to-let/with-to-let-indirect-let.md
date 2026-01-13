# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered for outer `with` when there's an indirect nested
`with` inside a `let` binding.

This is the "indirect nested with" case that the semantic check must handle.
Converting the outer `with` to `let/inherit` would change variable resolution
because `let` bindings shadow inner `with` scopes.

See: https://github.com/nix-community/nixd/pull/768#discussion_r2681198142

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

```nix file:///with-to-let-indirect-let.nix
with outer; let x = with inner; foo; in x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-indirect-let.nix"
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

No "Convert with to let/inherit" action should be offered when there's an
indirect nested `with` inside a `let` binding. The variable `foo` could
resolve from either `outer` or `inner` at evaluation time.

```
     CHECK:   "id": 2,
CHECK-NOT:   "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
