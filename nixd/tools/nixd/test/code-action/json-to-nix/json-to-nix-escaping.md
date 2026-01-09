# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action properly escapes special characters in strings.

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

```nix file:///json-to-nix-escaping.nix
{"key": "line1\nline2\ttab\"quote\\backslash"}
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-escaping.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":46
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

String escaping should convert JSON escapes to Nix escapes.

```
     CHECK:   "id": 1,
     CHECK:   "result": [
     CHECK:       "kind": "refactor.rewrite"
     CHECK:       "title": "Convert JSON to Nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
