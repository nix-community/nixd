
# RUN: nixd --lit-test < %s | FileCheck %s

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

```nix file:///rewrite-string.nix
"hello"
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///rewrite-string.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":7
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

```
     CHECK:   "id": 1,
     CHECK:   "jsonrpc": "2.0",
     CHECK:   "result": [
     CHECK:     {
     CHECK:       "edit": {
     CHECK:         "changes": {
     CHECK:           "file:///rewrite-string.nix": [
     CHECK:             {
     CHECK:               "newText": "''hello''",
     CHECK:               "range": {
     CHECK:                 "end": {
     CHECK:                   "character": 7,
     CHECK:                   "line": 0
     CHECK:                 },
     CHECK:                 "start": {
     CHECK:                   "character": 0,
     CHECK:                   "line": 0
     CHECK:                 }
     CHECK:               }
     CHECK:             }
     CHECK:           ]
     CHECK:         }
     CHECK:       },
     CHECK:       "kind": "refactor.rewrite",
     CHECK:       "title": "Convert to indented string"
     CHECK:     }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
