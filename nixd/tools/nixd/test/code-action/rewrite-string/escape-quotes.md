# RUN: nixd --lit-test < %s | FileCheck %s

Test that quote characters are properly escaped when converting formats.

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

Test converting indented string with double quotes to double-quoted string.

```nix file:///escape-quotes.nix
{ x = ''say "hi"''; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///escape-quotes.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":18
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The double quotes inside should be escaped as \"

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "\"say \\\"hi\\\"\""
     CHECK:       "title": "Convert to double-quoted string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
