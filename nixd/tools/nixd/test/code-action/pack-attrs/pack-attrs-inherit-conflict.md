# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action is NOT offered when an inherit statement conflicts with dotted path prefix.

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

```nix file:///pack-attrs-inherit-conflict.nix
{ inherit foo; foo.bar = 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-inherit-conflict.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":15
         },
         "end":{
            "line":0,
            "character":22
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No Pack action should be offered when inherit statement (inherit foo) conflicts with dotted path (foo.bar).

```
     CHECK:   "id": 2,
     CHECK:   "result": []
CHECK-NOT:    "Pack"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
