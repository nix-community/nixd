# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered when cursor is NOT on the `with` keyword.

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

```nix file:///with-to-let-cursor.nix
with lib; optionalString foo "yes"
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///with-to-let-cursor.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":10
         },
         "end":{
            "line":0,
            "character":24
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Cursor is on "optionalString" (characters 10-24), not on "with" keyword.
No action should be offered.

```
     CHECK:   "id": 2,
CHECK-NOT:   "Convert `with` to `let/inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
