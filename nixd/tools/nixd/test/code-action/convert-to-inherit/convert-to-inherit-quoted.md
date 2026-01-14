# RUN: nixd --lit-test < %s | FileCheck %s

# Test convert-to-inherit with quoted attribute name containing hyphen
# Note: foo-bar is a valid Nix identifier (hyphens allowed), so no quotes needed
# { "foo-bar" = foo-bar; } -> { inherit foo-bar; }

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

```nix file:///convert-to-inherit-quoted.nix
{ "foo-bar" = foo-bar; }
```

<-- textDocument/codeAction(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/codeAction",
   "params":{
      "textDocument":{
         "uri":"file:///convert-to-inherit-quoted.nix"
      },
      "range":{
         "start":{
            "line": 0,
            "character":2
         },
         "end":{
            "line":0,
            "character":22
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
CHECK: "newText": "inherit foo-bar;"
CHECK: "kind": "refactor.rewrite"
CHECK: "title": "Convert to `inherit`"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
