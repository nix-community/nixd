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

```nix file:///noogle-lib-nested.nix
lib.strings.optionalString
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///noogle-lib-nested.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":12
         },
         "end":{
            "line":0,
            "character":26
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
     CHECK:   "data": {
CHECK-NEXT:     "noogleUrl": "https://noogle.dev/f/lib/strings/optionalString"
CHECK-NEXT:   },
CHECK-NEXT:   "kind": "refactor",
CHECK-NEXT:   "title": "Open Noogle documentation for optionalString"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
