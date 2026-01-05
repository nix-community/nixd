# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action is NOT offered when sibling bindings contain dynamic attributes.

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

```nix file:///pack-attrs-sibling-dynamic.nix
let x = "baz"; in { foo.bar = 1; foo.${x} = 2; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-sibling-dynamic.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":20
         },
         "end":{
            "line":0,
            "character":27
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No Pack action should be offered when sibling has dynamic attribute (foo.${x}).

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": []
CHECK-NOT:    "Pack"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
