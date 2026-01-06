# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Pack all (shallow)" action preserves nested dotted paths instead of recursively expanding them.

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

```nix file:///pack-attrs-shallow-bulk.nix
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
         "uri":"file:///pack-attrs-shallow-bulk.nix"
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

Shallow Pack All should preserve "bar.x" as dotted path (not expand to "bar = { x = 1; }").

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One - packs only foo.bar.x

```
     CHECK:       "newText": "foo = { bar.x = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All - preserves bar.x as dotted path

```
     CHECK:       "newText": "foo = { bar.x = 1; baz = 2; };"
     CHECK:       "title": "Pack all 'foo' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All - fully nests bar = { x = 1; }

```
     CHECK:       "newText": "foo = { bar = { x = 1; }; baz = 2; };"
     CHECK:       "title": "Recursively pack all 'foo' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
