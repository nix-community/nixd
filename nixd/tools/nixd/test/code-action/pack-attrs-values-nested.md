# RUN: nixd --lit-test < %s | FileCheck %s

Test that both Flatten and Pack actions are offered for nested attribute set values.

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

```nix file:///pack-attrs-values-nested.nix
{ nested.value = { a = 1; b.c = 2; }; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-values-nested.nix"
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

For nested attribute set values, both Flatten and Pack actions are offered.
Flatten transforms `nested.value = { a = 1; b.c = 2; }` to `nested.value.a = 1; nested.value.b.c = 2;`
Pack transforms it to `nested = { value = { a = 1; b.c = 2; }; }`

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "nested.value.a = 1; nested.value.b.c = 2;"
     CHECK:       "title": "Flatten nested attribute set"
CHECK-NEXT:     },
CHECK-NEXT:     {
     CHECK:       "newText": "nested = { value = { a = 1; b.c = 2; }; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
