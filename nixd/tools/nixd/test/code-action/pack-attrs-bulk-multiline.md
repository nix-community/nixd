# RUN: nixd --lit-test < %s | FileCheck %s

Test that bulk Pack action works correctly with multiline attribute sets.

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

```nix file:///pack-attrs-bulk-multiline.nix
{
  foo.bar = 1;
  foo.baz = 2;
  foo.qux = 3;
}
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-bulk-multiline.nix"
      },
      "range":{
         "start":{
            "line": 1,
            "character":2
         },
         "end":{
            "line":1,
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

Three Pack actions should be offered for multiline input.
The bulk actions should span from line 1 to line 3.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One - only the current binding on line 1

```
     CHECK:       "newText": "foo = { bar = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All - spans lines 1-3

```
     CHECK:       "newText": "foo = { bar = 1; baz = 2; qux = 3; };"
     CHECK:       "title": "Pack all 'foo' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All - same span, fully nested

```
     CHECK:       "newText": "foo = { bar = 1; baz = 2; qux = 3; };"
     CHECK:       "title": "Recursively pack all 'foo' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
