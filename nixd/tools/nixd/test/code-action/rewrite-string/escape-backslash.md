# RUN: nixd --lit-test < %s | FileCheck %s

Test that backslash escapes are properly handled when converting to indented string.

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

```nix file:///escape-backslash.nix
{ x = "path\\to\\file"; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///escape-backslash.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":22
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The backslashes are unescaped to literal backslashes in the indented string.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "''path\\to\\file''"
     CHECK:       "title": "Convert to indented string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
