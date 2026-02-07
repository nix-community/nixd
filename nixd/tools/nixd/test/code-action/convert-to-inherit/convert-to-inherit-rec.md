# RUN: nixd --lit-test < %s | FileCheck %s

# Test that convert-to-inherit action IS offered in rec attribute sets
# Note: Converting { foo = foo; } to { inherit foo; } is still valid in rec {}
# because inherit takes from lexical scope, not from the rec set itself.

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

```nix file:///convert-to-inherit-rec.nix
rec { foo = foo; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-rec.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":6
         },
         "end":{
            "line":0,
            "character":16
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
CHECK-NEXT:   "result": [
```

```
CHECK: "newText": "inherit foo;"
CHECK: "kind": "refactor.rewrite"
CHECK: "title": "Convert to `inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
