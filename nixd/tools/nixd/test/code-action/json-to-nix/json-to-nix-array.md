# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action handles arrays correctly.

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

```nix file:///json-to-nix-array.nix
[1, 2, 3]
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-array.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
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

JSON arrays should convert to Nix lists.

```
     CHECK:   "id": 1,
     CHECK:   "result": [
     CHECK:       "newText":
     CHECK:       1
     CHECK:       2
     CHECK:       3
     CHECK:       "title": "Convert JSON to Nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
