# RUN: nixd --lit-test < %s | FileCheck %s

Test that NO action is offered when lambda uses simple arg style `x: body` without formals.

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

```nix file:///no-formals.nix
x: y
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///no-formals.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 3
         },
         "end":{
            "line":0,
            "character": 4
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Simple lambda `x: y` has no formals set (curly brace style), so no add-to-formals action should be offered.

```
     CHECK:   "id": 2,
CHECK-NOT:   "add `y` to formals"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
