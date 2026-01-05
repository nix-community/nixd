# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action works correctly with various non-conflicting sibling bindings:
- Sibling with inherit (no path conflict)
- Siblings with different prefixes (no path conflict)

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


<-- textDocument/didOpen (inherit sibling)

```nix file:///pack-attrs-siblings-positive-1.nix
{ foo.bar = 1; inherit baz; }
```

<-- textDocument/codeAction(2) - Test with inherit sibling

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-siblings-positive-1.nix"
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

Pack action should be offered when sibling is inherit (no path conflict).

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK:        "newText": "foo = { bar = 1; };"
CHECK:        "title": "Pack dotted path to nested set"
```

<-- textDocument/didOpen (different prefix siblings)

```nix file:///pack-attrs-siblings-positive-2.nix
{ foo.bar = 1; baz.qux = 2; }
```

<-- textDocument/codeAction(3) - Test with different prefix siblings

```json
{
   "jsonrpc":"2.0",
   "id":3,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-siblings-positive-2.nix"
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

Pack action should transform only the selected foo.bar binding.

```
     CHECK:   "id": 3,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK:        "newText": "foo = { bar = 1; };"
CHECK:        "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
