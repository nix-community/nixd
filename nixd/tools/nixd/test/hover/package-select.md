# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ foo.meta.description = \"Very Nice\";  }" \
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


```nix file:///test.nix
pkgs.foo
```

<-- textDocument/hover(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{
         "uri":"file:///test.nix"
      },
      "position":{
         "line":0,
         "character":6
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
CHECK-NEXT:      "value": "## Description\n\nVery Nice\n\n"
CHECK-NEXT:    },
CHECK-NEXT:    "range": {
CHECK-NEXT:      "end": {
CHECK-NEXT:        "character": 8,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      },
CHECK-NEXT:      "start": {
CHECK-NEXT:        "character": 0,
CHECK-NEXT:        "line": 0
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  }
```
