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
{  a.${a.crash} = 1; }
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
         "character":10
      }
   }
}
```

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": null
CHECK-NEXT:  }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

