# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action is NOT offered when a non-path binding conflicts with dotted path.

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

```nix file:///pack-attrs-conflict.nix
{ foo = 1; foo.bar = 2; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-conflict.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":11
         },
         "end":{
            "line":0,
            "character":18
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No Pack action should be offered when a non-path binding (foo = 1) conflicts with dotted path (foo.bar).

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": []
CHECK-NOT:    "Pack"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
