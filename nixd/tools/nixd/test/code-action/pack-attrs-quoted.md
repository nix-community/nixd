# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action preserves quoted attribute names.

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

```nix file:///pack-attrs-quoted.nix
{ "foo-bar".baz = 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-quoted.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":14
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Pack action should preserve the quoted first segment.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
CHECK-NEXT:       "edit": {
CHECK-NEXT:         "changes": {
CHECK-NEXT:           "file:///pack-attrs-quoted.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": "\"foo-bar\" = { baz = 1; };",
CHECK-NEXT:               "range": {
CHECK-NEXT:                 "end": {
CHECK-NEXT:                   "character": 20,
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
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
