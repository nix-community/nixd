# RUN: nixd --lit-test \
# RUN: --nixos-options-expr="{ foo.bar = { _type = \"option\"; }; }" \
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
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"{ foo.bar = 1 }"
      }
   }
}
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
         "character":6
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
CHECK-NEXT:     "value": "`Identifer`"
CHECK-NEXT:   },
CHECK-NEXT:   "range": {
CHECK-NEXT:     "end": {
CHECK-NEXT:       "character": 9,
CHECK-NEXT:       "line": 0
CHECK-NEXT:     },
CHECK-NEXT:     "start": {
CHECK-NEXT:       "character": 6,
CHECK-NEXT:       "line": 0
CHECK-NEXT:     }
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
