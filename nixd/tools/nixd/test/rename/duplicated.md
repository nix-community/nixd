# RUN: nixd --lit-test < %s | FileCheck %s

// https://github.com/nix-community/nixd/issues/255

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
rec { bar = 1; a = 1; a = bar; b = a; }
```

<-- textDocument/rename(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/rename",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      },
      "position":{
        "line": 0,
        "character":28
      },
      "newName": "b"
   }
}
```

```
     CHECK: "error": {
CHECK-NEXT:   "code": -32001,
CHECK-NEXT:   "message": "no such variable"
CHECK-NEXT: },
CHECK-NEXT: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
