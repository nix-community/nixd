# RUN: nixd \
# RUN: --nixos-options-expr="{ foo.declarationPositions = [ { file = \"/foo\"; line = 8; column = 7; } ];  }" \
# RUN: --lit-test < %s | FileCheck %s

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
{ foo = 1; }
```

<-- textDocument/definition(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/definition",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
        "line": 0,
        "character":3
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "range": {
CHECK-NEXT:     "end": {
CHECK-NEXT:       "character": 6,
CHECK-NEXT:       "line": 7
CHECK-NEXT:     },
CHECK-NEXT:     "start": {
CHECK-NEXT:       "character": 6,
CHECK-NEXT:       "line": 7
CHECK-NEXT:     }
CHECK-NEXT:   },
CHECK-NEXT:   "uri": "file:///foo"
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
