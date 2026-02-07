# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Extract to file" action is offered for attribute set expressions.

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
         "workspace": {
            "workspaceEdit": {
               "documentChanges": true,
               "resourceOperations": ["create", "rename", "delete"]
            }
         }
      },
      "trace":"off"
   }
}
```


<-- textDocument/didOpen

```nix file:///test.nix
{ buildInputs = { foo = 1; bar = 2; }; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///test.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":16
         },
         "end":{
            "line":0,
            "character":35
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The Extract action should be offered for the attribute set.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "kind": "refactor"
     CHECK:       "title": "Extract to buildInputs.nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
