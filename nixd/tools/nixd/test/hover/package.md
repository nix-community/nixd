# RUN: nixd --lit-test \
# RUN: --nixpkgs-expr="{ hello.meta.description = \"Very Nice\";  }" \
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
with pkgs;  hello
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
         "character":16
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "contents": {
CHECK-NEXT:     "kind": "markdown",
CHECK-NEXT:     "value": "## Description\n\nVery Nice\n\n"
CHECK-NEXT:   },
CHECK-NEXT:   "range": {
CHECK-NEXT:     "end": {
CHECK-NEXT:       "character": 17,
CHECK-NEXT:       "line": 0
CHECK-NEXT:     },
CHECK-NEXT:     "start": {
CHECK-NEXT:       "character": 12,
CHECK-NEXT:       "line": 0
CHECK-NEXT:     }
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
