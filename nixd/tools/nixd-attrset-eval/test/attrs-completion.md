# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "{ py1 = 1; py2 = 2; py3 = 3; }"
}
```


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/attrpathComplete",
   "params": {
        "Scope": [ ],
        "Prefix": "py"
   }
}
```

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     "py1",
CHECK-NEXT:     "py2",
CHECK-NEXT:     "py3"
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

