# RUN: nixd --lit-test < %s | FileCheck %s

Test that flatten action is NOT offered when nested attrset contains inherit.

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

```nix file:///flatten-attrs-inherit.nix
{ foo = { inherit bar; }; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///flatten-attrs-inherit.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Attrsets with inherit should not offer flatten action.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
 CHECK-NOT:   "Flatten nested attribute set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
