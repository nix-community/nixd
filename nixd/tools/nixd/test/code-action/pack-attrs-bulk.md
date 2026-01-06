# RUN: nixd --lit-test < %s | FileCheck %s

Test that bulk Pack action is offered when multiple sibling bindings share the same prefix.

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

```nix file:///pack-attrs-bulk.nix
{ foo.bar = 1; foo.baz = 2; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-bulk.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":9
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Three Pack actions should be offered:
1. Pack One - only the current binding
2. Shallow Pack All - all 'foo' bindings, one level
3. Recursive Pack All - all 'foo' bindings, fully nested

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
CHECK-NEXT:       "edit": {
CHECK-NEXT:         "changes": {
CHECK-NEXT:           "file:///pack-attrs-bulk.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": "foo = { bar = 1; };",
CHECK-NEXT:               "range": {
CHECK-NEXT:                 "end": {
CHECK-NEXT:                   "character": 14,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 },
CHECK-NEXT:                 "start": {
CHECK-NEXT:                   "character": 2,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 }
CHECK-NEXT:               }
CHECK-NEXT:             }
CHECK-NEXT:           ]
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK-NEXT:       "edit": {
CHECK-NEXT:         "changes": {
CHECK-NEXT:           "file:///pack-attrs-bulk.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": "foo = { bar = 1; baz = 2; };",
CHECK-NEXT:               "range": {
CHECK-NEXT:                 "end": {
CHECK-NEXT:                   "character": 27,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 },
CHECK-NEXT:                 "start": {
CHECK-NEXT:                   "character": 2,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 }
CHECK-NEXT:               }
CHECK-NEXT:             }
CHECK-NEXT:           ]
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Pack all 'foo' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
CHECK-NEXT:       "edit": {
CHECK-NEXT:         "changes": {
CHECK-NEXT:           "file:///pack-attrs-bulk.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": "foo = { bar = 1; baz = 2; };",
CHECK-NEXT:               "range": {
CHECK-NEXT:                 "end": {
CHECK-NEXT:                   "character": 27,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 },
CHECK-NEXT:                 "start": {
CHECK-NEXT:                   "character": 2,
CHECK-NEXT:                   "line": 0
CHECK-NEXT:                 }
CHECK-NEXT:               }
CHECK-NEXT:             }
CHECK-NEXT:           ]
CHECK-NEXT:         }
CHECK-NEXT:       },
CHECK-NEXT:       "kind": "refactor.rewrite",
CHECK-NEXT:       "title": "Recursively pack all 'foo' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
