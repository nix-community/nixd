# RUN: nixd --lit-test < %s | FileCheck %s

Test that newline escapes are properly converted between string formats.

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

```nix file:///escape-newline.nix
{ x = "hello\nworld"; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///escape-newline.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":20
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The converted indented string contains actual newlines (shown as \n in JSON).

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "''hello\nworld''"
     CHECK:       "title": "Convert to indented string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
