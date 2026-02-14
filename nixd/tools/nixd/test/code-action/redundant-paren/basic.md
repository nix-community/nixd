# RUN: nixd --lit-test < %s | FileCheck %s

Test basic `remove redundant parens` action for unnecessary parentheses.

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

```nix file:///redundant-paren.nix
(1)
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///redundant-paren.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 0
         },
         "end":{
            "line":0,
            "character": 1
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should remove both `(` and `)` since the inner expression `1` is simple.

```
     CHECK: "id": 2,
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "character": 1,
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "character": 0,
     CHECK:     "line": 0
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "character": 3,
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "character": 2,
     CHECK:     "line": 0
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove ( and )"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
