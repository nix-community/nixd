# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "{ hello.meta.description = \"A program that produces a familiar, friendly greeting\"; }"
}
```


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/attrpathInfo",
   "params": [ "hello" ]
}
```

```
     CHECK: "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "Description": "A program that produces a familiar, friendly greeting",
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

