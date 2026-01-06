# RUN: nixd --lit-test < %s | FileCheck %s

Test that all three pack actions are offered for multi-sibling case in the correct order:
1. Pack dotted path to nested set (Pack One)
2. Pack all 'X' bindings to nested set (Shallow)
3. Recursively pack all 'X' bindings to nested set (Recursive)

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

```nix file:///pack-attrs-multi-actions.nix
{ a.b = 1; a.c = 2; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-multi-actions.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":6
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

All three actions should be present in order.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One

```
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All

```
     CHECK:       "title": "Pack all 'a' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All

```
     CHECK:       "title": "Recursively pack all 'a' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
