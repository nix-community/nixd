# RUN: nixd --lit-test < %s | FileCheck %s

Test that Pack action packs only one level for deep dotted paths.

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

```nix file:///pack-attrs-deep.nix
{ a.b.c = 1; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///pack-attrs-deep.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
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

Pack should convert only one level: a.b.c -> a = { b.c = 1; }

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "a = { b.c = 1; };"
     CHECK:       "title": "Pack dotted path to nested set"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
