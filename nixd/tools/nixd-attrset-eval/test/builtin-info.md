# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/builtinInfo",
   "params": [ "any" ]
}
```

```
     CHECK:  "id": 1,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": {
CHECK-NEXT:    "arity": 2,
CHECK-NEXT:    "doc": "{{.*}}"
CHECK-NEXT:  }
CHECK-NEXT:}
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
