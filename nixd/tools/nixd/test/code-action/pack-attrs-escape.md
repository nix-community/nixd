# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action correctly escapes special characters in quoted attribute keys.
Tests escaping of: double quotes ("), backslashes (\), and dollar signs ($).

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

Bulk pack with keys containing special characters that need escaping.

```nix file:///pack-attrs-escape.nix
{ "has\"quote".a = 1; "has\"quote".b = 2; }
```

<-- textDocument/codeAction(2)

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-escape.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":17
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Bulk Pack action should correctly escape the double quote in the key.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "\"has\\\"quote\" = { a = 1; b = 2; };"
     CHECK:       "title": "Pack all 'has\"quote' bindings to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
