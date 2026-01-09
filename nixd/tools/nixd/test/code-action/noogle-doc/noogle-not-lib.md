# RUN: nixd --lit-test < %s | FileCheck %s

# Test that Noogle action is NOT offered for non-lib selects

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

```nix file:///noogle-not-lib.nix
pkgs.hello
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///noogle-not-lib.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":5
         },
         "end":{
            "line":0,
            "character":10
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
CHECK-NEXT:   "jsonrpc": "2.0",
 CHECK-NOT:   "noogleUrl"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
