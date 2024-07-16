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
         "text":"{ inherit (builtins) concatLists listToAttrs filter attrNames; }"
      }
   }
}
```

<-- textDocument/semanticTokens(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/semanticTokens/full",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      }
   }
}
```

```
     CHECK: "id": 2,
CHECK-NEXT: "jsonrpc": "2.0",
CHECK-NEXT: "result": {
CHECK-NEXT:   "data": [],
CHECK-NEXT:   "resultId": ""
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
