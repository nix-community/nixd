# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered for outer `with` when the inner `with` is parenthesized.

The `unwrapParen()` helper correctly unwraps parentheses to detect nested `with`,
so `with a; (with b; x)` is treated the same as `with a; with b; x`.

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

```nix file:///with-to-let-nested-paren.nix
with outer; (with inner; foo)
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-nested-paren.nix"
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
on the outer `with`, even when the inner `with` is wrapped in parentheses.

```
     CHECK:   "id": 2,
CHECK-NOT:   "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
