# RUN: nixd --lit-test < %s | FileCheck %s

Test that bulk Pack action correctly handles non-contiguous sibling bindings with the same prefix.

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

```nix file:///pack-attrs-bulk-non-contiguous.nix
{ foo.a = 1; bar = 2; foo.b = 3; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-bulk-non-contiguous.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":8
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
1. Pack One - only the current binding (foo.a)
2. Shallow Pack All - all 'foo' bindings (foo.a and foo.b), even though they are separated by 'bar'
3. Recursive Pack All - all 'foo' bindings, fully nested

```
     CHECK:   "id": 2,
     CHECK:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One - only foo.a

```
     CHECK:       "newText": "foo = { a = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All - groups non-contiguous foo.a and foo.b bindings

```
     CHECK:       "newText": "foo = { a = 1; b = 3; };"
     CHECK:       "title": "Pack all 'foo' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All - same as shallow for single-level paths

```
     CHECK:       "newText": "foo = { a = 1; b = 3; };"
     CHECK:       "title": "Recursively pack all 'foo' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
