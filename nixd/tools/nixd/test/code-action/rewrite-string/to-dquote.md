# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert to double-quoted string" action is offered for indented strings.

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

```nix file:///to-dquote.nix
{ x = ''hello''; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///to-dquote.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":15
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
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "\"hello\""
     CHECK:       "title": "Convert to double-quoted string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
