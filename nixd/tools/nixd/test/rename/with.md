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

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix",
         "languageId":"nix",
         "version":1,
         "text":"with builtins; aa"
      }
   }
}
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
        "character":16
      },
      "newName": "b"
   }
}
```

```
     CHECK:   "message": "cannot rename `with` defined variable"
CHECK-NEXT: },
CHECK-NEXT: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0"
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
