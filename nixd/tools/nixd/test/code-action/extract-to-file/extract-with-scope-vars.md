# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Extract to file" warns about 'with' scope variables in the title.

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
{ pkgs }: with pkgs; { buildInputs = [ foo bar ]; }
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
            "character":37
         },
         "end":{
            "line":0,
            "character":48
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The Extract action should detect 'with' scope variables and warn about them.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "kind": "refactor"
     CHECK:       "title": "Extract to buildInputs.nix (2 free variables, has 'with' vars)"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
