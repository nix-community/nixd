# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "fuck $$@"
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
     CHECK:    "message": "value is invalid"
CHECK-NEXT:  },
CHECK-NEXT:  "id": 1,
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

