# RUN: nixd --lit-test < %s | FileCheck %s

Test that tab escapes are properly handled when converting to indented string.

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

```nix file:///escape-tab.nix
{ x = "col1\tcol2"; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///escape-tab.nix"
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

The tab escape becomes a literal tab character in the indented string (shown as \t in JSON).

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "newText": "''col1\tcol2''"
     CHECK:       "title": "Convert to indented string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
