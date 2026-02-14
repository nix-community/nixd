# RUN: nixd --lit-test < %s | FileCheck %s

Test that `remove redundant parens` is NOT offered when parentheses are needed.

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

```nix file:///needed-paren.nix
(1 + 2)
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///needed-paren.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character": 0
         },
         "end":{
            "line":0,
            "character": 1
         }
      },
      "context":{
         "diagnostics":[],
         "triggerKind":2
      }
   }
}
```

No code action should be offered because `1 + 2` is a binary operation, making parentheses meaningful.

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": []
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
