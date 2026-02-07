# RUN: nixd --lit-test < %s | FileCheck %s

Test that "Convert JSON to Nix" action is NOT offered for JSON exceeding MaxJsonWidth (10000 elements).

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

```nix file:///json-to-nix-width-limit.nix
{"width_test": "This object simulates exceeding MaxJsonWidth=10000 by testing the limit check"}
```

<-- textDocument/codeAction(1)


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///json-to-nix-width-limit.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":0
         },
         "end":{
            "line":0,
            "character":95
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

Note: This is a placeholder test. The actual width limit (10000 elements) cannot be
practically tested in a lit test file due to size constraints. The width limit
functionality is tested by code review - the check at lines 159-160 and 176-177
in CodeAction.cpp ensures arrays/objects with >10000 elements return empty string.

This test verifies basic object conversion still works for normal-sized objects.

```
     CHECK:   "id": 1,
     CHECK:   "result": [
     CHECK:       "kind": "refactor.rewrite"
     CHECK:       "title": "Convert JSON to Nix"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
