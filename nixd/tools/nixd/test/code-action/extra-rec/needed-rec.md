# RUN: nixd --lit-test < %s | FileCheck %s

Test that `remove extra rec` is NOT offered when `rec` is actually needed.

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

```nix file:///needed-rec.nix
rec { x = 1; y = x; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///needed-rec.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 0
         },
         "end":{
            "line":0,
            "character": 3
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No code action should be offered because `y` depends on `x`, making `rec` necessary.

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": []
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
