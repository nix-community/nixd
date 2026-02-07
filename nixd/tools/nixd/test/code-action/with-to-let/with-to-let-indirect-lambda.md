# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered for outer `with` when there's an indirect nested
`with` inside a lambda body.

This is another "indirect nested with" case. The inner `with` is not directly
in the body of the outer `with`, but inside a lambda expression.

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

```nix file:///with-to-let-indirect-lambda.nix
with outer; x: with inner; baz
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-indirect-lambda.nix"
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
indirect nested `with` inside a lambda. The variable `baz` could resolve
from either `outer` or `inner` at evaluation time.

```
     CHECK:   "id": 2,
CHECK-NOT:   "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
