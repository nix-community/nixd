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

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///with.nix",
         "languageId":"nix",
         "version":1,
         "text":"let x=1; in    x"
      }
   }
}
```

<-- textDocument/formatting

```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"textDocument/formatting",
   "params":{
      "textDocument":{
         "uri":"file:///with.nix"
      },
   }
}
```

```json
{"jsonrpc":"2.0","method":"exit"}
```