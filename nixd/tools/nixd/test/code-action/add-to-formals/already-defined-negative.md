# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered when the variable is already defined in formals.

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

```nix file:///already-defined.nix
{x}: x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///already-defined.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 5
         },
         "end":{
            "line":0,
            "character": 6
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Variable `x` is already defined in the formals, so no add-to-formals action should be offered.

```
     CHECK:   "id": 2,
CHECK-NOT:   "add `x` to formals"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
