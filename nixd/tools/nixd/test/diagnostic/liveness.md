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

```nix file:///basic.nix
x: y: x + 1
```

```
     CHECK: "diagnostics": [],
CHECK-NEXT: "uri": "file:///basic.nix",
CHECK-NEXT: "version": 1
```
