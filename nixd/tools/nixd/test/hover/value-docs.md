# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ myFunc = /** Documentation for myFunc */ x: y: x + y; }" \
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

```nix file:///basic.nix
pkgs.myFunc
```

<-- textDocument/hover(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/hover",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
         "line":0,
         "character":5
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "contents": {
CHECK-NEXT:       "kind": "markdown",
CHECK-NEXT:       "value": "## Value\n\nDocumentation for myFunc \n\n\n**Arity:** 0\n"
CHECK-NEXT:     },
CHECK-NEXT:     "range": {
CHECK-NEXT:       "end": {
CHECK-NEXT:         "character": 11,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       },
CHECK-NEXT:       "start": {
CHECK-NEXT:         "character": 0,
CHECK-NEXT:         "line": 0
CHECK-NEXT:       }
CHECK-NEXT:     }
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

