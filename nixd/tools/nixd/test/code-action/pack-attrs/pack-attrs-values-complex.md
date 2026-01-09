# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action correctly preserves complex let expression values.

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

```nix file:///pack-attrs-values-complex.nix
{ complex.value = let x = 1; in x + 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-values-complex.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":15
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Pack action should preserve complex let expressions.
No Flatten action since the value is not an ExprAttrs.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "complex = { value = let x = 1; in x + 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
