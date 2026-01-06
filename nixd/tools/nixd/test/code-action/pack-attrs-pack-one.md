# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Pack One" action is offered when multiple sibling bindings share the same prefix.
This allows users to pack only the current binding while leaving siblings unchanged.

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

```nix file:///pack-attrs-pack-one.nix
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
         "uri":"file:///pack-attrs-pack-one.nix"
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

Three Pack actions should be offered when multiple sibling bindings share the same prefix.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One - packs only a.b, leaving a.c unchanged

```
     CHECK:       "newText": "a = { b = 1; };"
     CHECK:               "end": {
CHECK-NEXT:                   "character": 10,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 },
CHECK-NEXT:                 "start": {
CHECK-NEXT:                   "character": 2,
CHECK-NEXT:                   "line": 0
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All - packs both a.b and a.c together

```
     CHECK:       "newText": "a = { b = 1; c = 2; };"
     CHECK:       "title": "Pack all 'a' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All - same as shallow for this flat case

```
     CHECK:       "newText": "a = { b = 1; c = 2; };"
     CHECK:       "title": "Recursively pack all 'a' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
