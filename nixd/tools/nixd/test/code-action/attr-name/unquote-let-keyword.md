# RUN: nixd --lit-test < %s | FileCheck %s

Test that keywords cannot be unquoted in let bindings (e.g., "true" should NOT offer unquote action)

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

```nix file:///unquote-let-keyword.nix
let "true" = 1; in x
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///unquote-let-keyword.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":4
         },
         "end":{
            "line":0,
            "character":10
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Keywords should NOT offer unquote action.
However, a quickfix for unused binding may still be offered.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NOT:   "title": "unquote attribute name"
     CHECK:   "title": "remove unused binding"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
