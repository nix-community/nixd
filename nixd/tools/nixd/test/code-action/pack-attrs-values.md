# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action correctly handles various value types:
- Empty attribute sets
- Complex expressions (let)
- Nested attribute sets

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


<-- textDocument/didOpen (empty value)

```nix file:///pack-attrs-values.nix
{ empty.value = { }; complex.value = let x = 1; in x + 1; nested.value = { a = 1; b.c = 2; }; }
```

<-- textDocument/codeAction(2) - Test empty attribute set value

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-values.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":13
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Pack action should be offered for empty attribute set values.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK:        "newText": "empty = { value = { }; };"
CHECK:        "title": "Pack dotted path to nested set"
```

<-- textDocument/codeAction(3) - Test complex let expression value

```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-values.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":21
         },
         "end":{
            "line":0,
            "character":34
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

```
     CHECK:   "id": 3,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK:        "newText": "complex = { value = let x = 1; in x + 1; };"
CHECK:        "title": "Pack dotted path to nested set"
```

<-- textDocument/codeAction(4) - Test nested attribute set value

```json
{
   "jsonrpc":"2.0",
   "id":4,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-values.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":58
         },
         "end":{
            "line":0,
            "character":70
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Pack action should preserve nested attribute set values exactly.

```
     CHECK:   "id": 4,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK:        "newText": "nested = { value = { a = 1; b.c = 2; }; };"
CHECK:        "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
