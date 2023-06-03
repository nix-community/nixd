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

Check that we handle relative paths corretly. Introduce `importee.nix` and `importor.nix` in our workspace, and test whether `importor.nix` can **import** `importee.nix`.

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///importee.nix",
         "languageId":"nix",
         "version":1,
         "text":"{a, b}: a + b"
      }
   }
}
```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///importor.nix",
         "languageId":"nix",
         "version":1,
         "text":"import ./importee.nix { a = 1; }"
      }
   }
}
```


```
CHECK: "message": "function 'anonymous lambda' called without required argument 'b'",
```




```json
{"jsonrpc":"2.0","method":"exit"}
```
