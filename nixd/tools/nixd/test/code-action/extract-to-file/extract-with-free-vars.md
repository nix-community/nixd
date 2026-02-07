# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Extract to file" detects free variables and includes them in the title.

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
{ pkgs }: { buildInputs = [ pkgs.foo pkgs.bar ]; }
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
            "character":26
         },
         "end":{
            "line":0,
            "character":47
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The Extract action should detect pkgs as a free variable.

```
     CHECK:   "id": 2,
     CHECK:   "result": [
     CHECK:       "kind": "refactor"
     CHECK:       "title": "Extract to buildInputs.nix (1 free variable)"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
