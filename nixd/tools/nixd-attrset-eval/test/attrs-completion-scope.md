# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "{ a = 1; llvmPackages = { clang = 1; clang-manpages = 1; }; }"
}
```


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/attrpathComplete",
   "params": {
        "Scope":  [ "llvmPackages" ],
        "Prefix": "cl"
   }
}
```

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": {
CHECK-NEXT:     "IsIncomplete": false,
CHECK-NEXT:     "Items": [
CHECK-NEXT:     "clang",
CHECK-NEXT:     "clang-manpages"
CHECK-NEXT:     ]
CHECK-NEXT:   }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
