# RUN: nixd --lit-test < %s | FileCheck %s

Test `remove redundant parens` action for nested parentheses `((1))`.

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

```nix file:///nested-paren.nix
((1))
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///nested-paren.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 0
         },
         "end":{
            "line":0,
            "character": 5
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Both inner and outer parentheses are redundant, so two quickfix actions should be offered.

```
     CHECK: "id": 2,
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove ( and )"
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove ( and )"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
