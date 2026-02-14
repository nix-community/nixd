# RUN: nixd --lit-test < %s | FileCheck %s

Test `remove extra rec` action for empty recursive attribute set.

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

```nix file:///empty-rec.nix
rec { }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///empty-rec.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 0
         },
         "end":{
            "line":0,
            "character": 3
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The action should remove the `rec` keyword since the attrset has no bindings at all.

```
     CHECK: "id": 2,
     CHECK: "newText": "",
     CHECK: "range":
     CHECK:   "end":
     CHECK:     "character": 3,
     CHECK:     "line": 0
     CHECK:   "start":
     CHECK:     "character": 0,
     CHECK:     "line": 0
     CHECK: "isPreferred": true,
     CHECK: "kind": "quickfix",
     CHECK: "title": "remove `rec` keyword"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
