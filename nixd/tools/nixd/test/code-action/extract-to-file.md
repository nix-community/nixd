
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

```nix file:///extract-to-file.nix
{ foo = { bar = 1; baz = 2; }; }
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///extract-to-file.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":8
         },
         "end":{
            "line":0,
            "character":28
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
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK:        "command": {
CHECK:          "arguments": [
CHECK:            "expression":
CHECK:            "range":
CHECK:            "uri": "file:///extract-to-file.nix"
CHECK:          "command": "nixd.extractToFile",
CHECK:          "title": "Extract expression to file"
CHECK:        "title": "Extract expression to file"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
