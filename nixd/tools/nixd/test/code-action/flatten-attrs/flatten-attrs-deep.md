# RUN: nixd --lit-test < %s | FileCheck %s

Test that flatten action works with already-dotted outer paths.

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

```nix file:///flatten-attrs-deep.nix
{ a.b = { c = 1; }; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///flatten-attrs-deep.nix"
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

No Quote action since "a.b" is a path, not a simple identifier.
Both Flatten and Pack actions are offered since "a.b" is a dotted path with nested value.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
CHECK:         "title": "Flatten nested attribute set"
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK:         "title": "Pack dotted path to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
