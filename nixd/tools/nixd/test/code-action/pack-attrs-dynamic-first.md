# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action is NOT offered when the FIRST segment of the path is dynamic.

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

```nix file:///pack-attrs-dynamic-first.nix
let x = "foo"; in { ${x}.bar = 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-dynamic-first.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":20
         },
         "end":{
            "line":0,
            "character":28
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No Pack action should be offered when the first segment is dynamic (interpolation).
Even if subsequent segments are static, the dynamic first segment prevents packing.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": []
CHECK-NOT:    "Pack"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
