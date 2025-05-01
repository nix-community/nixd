# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ foo = { _type = \"option\"; description = \"test option\"; type.description = \"test type\"; }; }" \
# RUN: < %s | FileCheck %s

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


```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///test.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ config = { foo }; }"
      }
   }
}
```

```json
{
   "jsonrpc": "2.0",
   "id": 2,
   "method": "textDocument/hover",
   "params":{
      "textDocument":{
         "uri": "file:///test.nix"
      },
      "position":{
         "line": 0,
         "character": 15
      }
   }
}
```

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "contents": {
CHECK-NEXT:      "kind": "markdown",
CHECK-NEXT:      "value": " (test type)\n\ntest option"
CHECK-NEXT:    },
CHECK-NEXT:    "range": {
CHECK-NEXT:      "end": {
CHECK-NEXT:        "character": 16,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      },
CHECK-NEXT:      "start": {
CHECK-NEXT:        "character": 13,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  }
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
