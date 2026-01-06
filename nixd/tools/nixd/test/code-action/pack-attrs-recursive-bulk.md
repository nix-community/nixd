# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Recursively pack all" action fully nests all dotted paths.

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

```nix file:///pack-attrs-recursive-bulk.nix
{ foo.bar.x = 1; foo.baz = 2; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-recursive-bulk.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":12
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Three actions should be offered. The third (Recursive Pack All) should fully expand "bar.x" to "bar = { x = 1; }".

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
CHECK-NEXT:       "edit": {
CHECK-NEXT:         "changes": {
CHECK-NEXT:           "file:///pack-attrs-recursive-bulk.nix": [
CHECK-NEXT:             {
CHECK-NEXT:               "newText": "foo = { bar.x = 1; };",
```

Skip to the third action (Recursive Pack All):

```
     CHECK:       "title": "Recursively pack all 'foo' bindings to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
