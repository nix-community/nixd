# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action handles all JSON types correctly.

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

```nix file:///json-to-nix-types.nix
{"str": "hello", "num": 42, "float": 3.14, "bool": true, "nil": null}
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-types.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":69
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

All JSON types should convert to their Nix equivalents.

```
     CHECK:   "id": 1,
     CHECK:   "result": [
     CHECK:       "kind": "refactor.rewrite"
     CHECK:       "title": "Convert JSON to Nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
