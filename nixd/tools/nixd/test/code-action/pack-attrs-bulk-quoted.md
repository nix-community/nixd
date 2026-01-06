# RUN: nixd --lit-test < %s | FileCheck %s

Test that bulk Pack action correctly handles keys requiring quotes.
The key "1foo" starts with a digit, requiring quotes in the output.

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

```nix file:///pack-attrs-bulk-quoted.nix
{ "1foo".a = 1; "1foo".b = 2; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-bulk-quoted.nix"
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

Three Pack actions should be offered with quoted key "1foo" since it starts with a digit.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     {
```

Action 1: Pack One - only the first quoted key binding

```
     CHECK:       "newText": "\"1foo\" = { a = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 2: Shallow Pack All - all quoted key bindings

```
     CHECK:       "newText": "\"1foo\" = { a = 1; b = 2; };"
     CHECK:       "title": "Pack all '1foo' bindings to nested set"
CHECK-NEXT:     },
CHECK-NEXT:     {
```

Action 3: Recursive Pack All - fully nested with quoted key

```
     CHECK:       "newText": "\"1foo\" = { a = 1; b = 2; };"
     CHECK:       "title": "Recursively pack all '1foo' bindings to nested set"
CHECK-NEXT:     }
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
