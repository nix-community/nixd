# RUN: nixd --lit-test < %s | FileCheck %s

Test that the rewrite string action is NOT offered when cursor is on a non-string node.

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

```nix file:///not-string.nix
{ x = 42; y = ./path; z = true; }
```

<-- textDocument/codeAction(2)

Request code action on the integer literal (not a string).

```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///not-string.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":8
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

The rewrite string action should NOT be offered for non-string nodes.

```
     CHECK:   "id": 2,
CHECK-NEXT:   "jsonrpc": "2.0",
 CHECK-NOT:   "Convert to indented string"
 CHECK-NOT:   "Convert to double-quoted string"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
